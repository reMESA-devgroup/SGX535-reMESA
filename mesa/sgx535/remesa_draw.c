/* SPDX-License-Identifier: MIT */
#include "remesa_draw.h"
#include "remesa_fixed_state.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

unsigned remesa_position_format_bytes(enum pipe_format format)
{
    switch (format) {
    case PIPE_FORMAT_R32G32_FLOAT: return 8;
    case PIPE_FORMAT_R32G32B32_FLOAT: return 12;
    case PIPE_FORMAT_R32G32B32A32_FLOAT: return 16;
    default: return 0;
    }
}
/* Gallium's absent components are (z=0,w=1). This is checked host attribute
 * conversion only; it generates no hardware shader instructions. */
static void fetch_position(float out[4], const void *source, unsigned bytes)
{
    out[0]=out[1]=out[2]=0.0f; out[3]=1.0f;
    memcpy(out,source,bytes);
}

static uint32_t read_index(const unsigned char *p, unsigned width)
{
    uint32_t value = 0;
    /* memcpy respects native Gallium index storage and avoids unaligned loads.
     * Widths smaller than uint32_t need their matching native integer type. */
    if (width == 1) return *p;
    if (width == 2) { uint16_t v; memcpy(&v, p, sizeof(v)); return v; }
    memcpy(&value, p, sizeof(value));
    return value;
}

int remesa_shader_draw_inputs_validate(const struct remesa_shader_draw_inputs *in,
                                      struct remesa_draw_plan *out)
{
    struct remesa_draw_plan plan = {0};
    uint64_t base, end, color_end;
    uint32_t min, max;
    unsigned fetch_bytes;
    if (!in || !out || !in->vertex_data || !in->vertex_bytes ||
        !in->count || in->count % 3 || !in->color_width || !in->color_height)
        return EINVAL;
    fetch_bytes = remesa_position_format_bytes(in->position_format);
    if (!fetch_bytes || !in->vertex_stride || in->position_input_slot ||
        in->count > REMESA_DRAW_MAX_INDICES ||
        in->color_format != PIPE_FORMAT_B8G8R8A8_UNORM ||
        in->cull_face > PIPE_FACE_FRONT_AND_BACK ||
        !remesa_viewport_swizzle_is_identity(&in->viewport))
        return ENOTSUP;
    for (unsigned axis=0; axis<3; ++axis)
        if (!isfinite(in->viewport.scale[axis]) ||
            !isfinite(in->viewport.translate[axis])) return EINVAL;
    if (in->scissor_enabled) {
        if (in->scissor.minx > in->scissor.maxx ||
            in->scissor.miny > in->scissor.maxy ||
            in->scissor.maxx > in->color_width ||
            in->scissor.maxy > in->color_height) return EINVAL;
    } else if (in->scissor.minx || in->scissor.miny ||
               in->scissor.maxx || in->scissor.maxy) return EINVAL;
    /* Packed pixels need only their actual final row. Trailing row padding
     * is storage, not an additional pixel fetch; no GPU alignment is assumed. */
    if ((uint64_t)in->color_width * 4 > in->color_stride) return EINVAL;
    color_end = (uint64_t)(in->color_height-1) * in->color_stride +
                (uint64_t)in->color_width * 4;
    if (color_end > SIZE_MAX) return EOVERFLOW;
    if (color_end > in->color_bytes) return EINVAL;
    if (in->vertex_bytes > UINTPTR_MAX-(uintptr_t)in->vertex_data)
        return EOVERFLOW;
    if (in->index_size) {
        if (in->index_size != 1 && in->index_size != 2 && in->index_size != 4)
            return EINVAL;
        if (!in->index_data || !in->index_bytes) return EINVAL;
        if (in->index_bytes > UINTPTR_MAX-(uintptr_t)in->index_data)
            return EOVERFLOW;
        plan.index_first_byte = (uint64_t)in->start * in->index_size;
        plan.index_end_byte = plan.index_first_byte +
                              (uint64_t)in->count * in->index_size;
        if (plan.index_end_byte > in->index_bytes) return EINVAL;
        min = UINT32_MAX; max = 0;
        for (unsigned i=0; i<in->count; ++i) {
            uint32_t raw = read_index((const unsigned char *)in->index_data +
                (size_t)plan.index_first_byte + (size_t)i * in->index_size,
                in->index_size);
            int64_t biased = (int64_t)raw + in->index_bias;
            if (biased < 0 || biased > UINT32_MAX) return EINVAL;
            if ((uint32_t)biased < min) min = (uint32_t)biased;
            if ((uint32_t)biased > max) max = (uint32_t)biased;
        }
    } else {
        /* The native handoff canonicalizes Gallium's unspecified union/bias
         * instead of exposing uninitialized fields to the backend. */
        if (in->index_data || in->index_bytes || in->index_bias) return EINVAL;
        end = (uint64_t)in->start + in->count;
        if (end > (uint64_t)UINT32_MAX+1) return EINVAL;
        min = in->start; max = (uint32_t)(end-1);
    }
    base = (uint64_t)in->vertex_offset + in->element_offset;
    if (base > in->vertex_bytes || in->vertex_bytes-base < fetch_bytes ||
        max > (in->vertex_bytes-base-fetch_bytes) / in->vertex_stride)
        return EINVAL;
    plan.primitive_count = in->count / 3;
    plan.min_vertex = min; plan.max_vertex = max;
    plan.vertex_end_byte = base + (uint64_t)max * in->vertex_stride + fetch_bytes;
    *out = plan;
    return 0;
}

struct remesa_shader_draw_snapshot {
    struct remesa_shader_draw_inputs inputs;
    struct remesa_draw_plan plan;
    unsigned char *bytes;
    size_t byte_length;
};

static int snapshot_overlap(const void *a,size_t an,const void *b,size_t bn)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(an>UINTPTR_MAX-x || bn>UINTPTR_MAX-y)return 1;
    return an && bn && (x<=y ? y-x<an : x-y<bn);
}

int remesa_shader_draw_snapshot_create(const struct remesa_shader_draw_inputs *in,
                                      struct remesa_shader_draw_snapshot **out)
{
    struct remesa_draw_plan plan;
    struct remesa_shader_draw_snapshot *snapshot;
    size_t vertex_bytes,index_bytes;
    if(!in || !out || snapshot_overlap(in,sizeof(*in),out,sizeof(*out)))return EINVAL;
    int error=remesa_shader_draw_inputs_validate(in,&plan);if(error)return error;
    if(snapshot_overlap(out,sizeof(*out),in->vertex_data,in->vertex_bytes) ||
       (in->index_size && snapshot_overlap(out,sizeof(*out),in->index_data,in->index_bytes)))
        return EINVAL;
    if(*out)return EBUSY;
    /* Preserve original offsets/indices by copying the exact required resource
     * prefixes. Declared trailing capacity is not copied or retained. */
    if(plan.vertex_end_byte>REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES ||
       plan.index_end_byte>REMESA_SHADER_DRAW_SNAPSHOT_MAX_BYTES-plan.vertex_end_byte)
        return E2BIG;
    vertex_bytes=(size_t)plan.vertex_end_byte;index_bytes=(size_t)plan.index_end_byte;
    snapshot=calloc(1,sizeof(*snapshot));if(!snapshot)return ENOMEM;
    snapshot->byte_length=vertex_bytes+index_bytes;
    snapshot->bytes=malloc(snapshot->byte_length);
    if(!snapshot->bytes){free(snapshot);return ENOMEM;}
    snapshot->inputs=*in;snapshot->plan=plan;
    memcpy(snapshot->bytes,in->vertex_data,vertex_bytes);
    snapshot->inputs.vertex_data=snapshot->bytes;
    snapshot->inputs.vertex_bytes=vertex_bytes;
    snapshot->inputs.index_bytes=index_bytes;
    snapshot->inputs.index_data=NULL;
    if(index_bytes) {
        memcpy(snapshot->bytes+vertex_bytes,in->index_data,index_bytes);
        snapshot->inputs.index_data=snapshot->bytes+vertex_bytes;
    }
    *out=snapshot;return 0;
}

int remesa_shader_draw_snapshot_inspect(const struct remesa_shader_draw_snapshot *snapshot,
 struct remesa_shader_draw_inputs *out,struct remesa_draw_plan *plan)
{
    if(!snapshot || !out || !plan ||
       snapshot_overlap(out,sizeof(*out),plan,sizeof(*plan)) ||
       snapshot_overlap(out,sizeof(*out),snapshot,sizeof(*snapshot)) ||
       snapshot_overlap(plan,sizeof(*plan),snapshot,sizeof(*snapshot)) ||
       snapshot_overlap(out,sizeof(*out),snapshot->bytes,snapshot->byte_length) ||
       snapshot_overlap(plan,sizeof(*plan),snapshot->bytes,snapshot->byte_length))return EINVAL;
    *out=snapshot->inputs;*plan=snapshot->plan;return 0;
}

void remesa_shader_draw_snapshot_destroy(struct remesa_shader_draw_snapshot *snapshot)
{
    if(snapshot){free(snapshot->bytes);free(snapshot);}
}

struct remesa_gears_geometry {
    struct remesa_gears_geometry_view view;
    unsigned char data[];
};

static int gears_attribute_validate(const struct remesa_gears_attribute *a,
                                    uint32_t max_vertex,unsigned normal)
{
    unsigned width=remesa_position_format_bytes(a->format);
    if(!width || (normal && width==8))return ENOTSUP;
    if(!a->data || a->bytes>UINTPTR_MAX-(uintptr_t)a->data)return EINVAL;
    /* Division avoids overflow even for an untrusted 64-bit base offset. */
    if(a->offset>a->bytes || width>a->bytes-a->offset)return EINVAL;
    if(a->stride && max_vertex>(a->bytes-a->offset-width)/a->stride)return EINVAL;
    return 0;
}

int remesa_gears_geometry_create(const struct pipe_draw_info *info,
 const struct pipe_draw_start_count_bias *draw,unsigned normal_from_attribute,
 const struct remesa_gears_attribute *position,const struct remesa_gears_attribute *normal,
 const void *indices,size_t index_bytes,struct remesa_gears_geometry **out)
{
    uint32_t count,max_vertex=0;
    uint64_t index_first=0;
    int error;
    if(!info||!draw||!position||!out||normal_from_attribute>1||
       (normal_from_attribute&&!normal))return EINVAL;
    if(snapshot_overlap(out,sizeof(*out),info,sizeof(*info))||
       snapshot_overlap(out,sizeof(*out),draw,sizeof(*draw))||
       snapshot_overlap(out,sizeof(*out),position,sizeof(*position))||
       snapshot_overlap(out,sizeof(*out),position->data,position->bytes)||
       (normal_from_attribute &&
        (snapshot_overlap(out,sizeof(*out),normal,sizeof(*normal))||
         snapshot_overlap(out,sizeof(*out),normal->data,normal->bytes)))||
       (info->index_size&&snapshot_overlap(out,sizeof(*out),indices,index_bytes)))return EINVAL;
    if(*out)return EBUSY;
    if(info->mode!=MESA_PRIM_TRIANGLES||info->start_instance||
       info->instance_count>1||(info->index_size&&info->primitive_restart))return ENOTSUP;
    count=draw->count-draw->count%3;
    if(!count||!info->instance_count)return ENODATA;
    if(count>REMESA_GEARS_GEOMETRY_MAX_VERTICES)return E2BIG;
    if(info->index_size) {
        if(info->index_size!=1&&info->index_size!=2&&info->index_size!=4)return EINVAL;
        if(!indices||index_bytes>UINTPTR_MAX-(uintptr_t)indices)return EINVAL;
        index_first=(uint64_t)draw->start*info->index_size;
        if(index_first+(uint64_t)count*info->index_size>index_bytes)return EINVAL;
        for(uint32_t i=0;i<count;i++) {
            uint32_t raw=read_index((const unsigned char *)indices+(size_t)index_first+
                                    (size_t)i*info->index_size,info->index_size);
            int64_t id=(int64_t)raw+draw->index_bias;
            if(id<0||id>UINT32_MAX || (info->index_bounds_valid &&
               (raw<info->min_index||raw>info->max_index)))return EINVAL;
            if((uint32_t)id>max_vertex)max_vertex=(uint32_t)id;
        }
    } else {
        uint64_t end=(uint64_t)draw->start+count;
        if(end>(uint64_t)UINT32_MAX+1)return EINVAL;
        max_vertex=(uint32_t)(end-1);
    }
    error=gears_attribute_validate(position,max_vertex,0);if(error)return error;
    if(normal_from_attribute) {
        error=gears_attribute_validate(normal,max_vertex,1);if(error)return error;
    }
    const uint32_t stride=normal_from_attribute?8:4;
    size_t bytes=(size_t)count*stride*4;
    struct remesa_gears_geometry *g=malloc(sizeof(*g)+bytes);
    if(!g)return ENOMEM;
    g->view=(struct remesa_gears_geometry_view){g->data,bytes,count,stride,normal_from_attribute};
    for(uint32_t i=0;i<count;i++) {
        uint32_t id=info->index_size ? (uint32_t)((int64_t)read_index(
          (const unsigned char *)indices+(size_t)index_first+(size_t)i*info->index_size,
          info->index_size)+draw->index_bias) : draw->start+i;
        for(unsigned a=0;a<1+normal_from_attribute;a++) {
            const struct remesa_gears_attribute *attr=a?normal:position;
            uint32_t value[4]={0,0,0,0x3f800000};
            memcpy(value,(const unsigned char *)attr->data+(size_t)attr->offset+
                         (size_t)id*attr->stride,remesa_position_format_bytes(attr->format));
            memcpy(g->data+((size_t)i*stride+a*4)*4,value,sizeof(value));
        }
    }
    *out=g;return 0;
}

int remesa_gears_geometry_inspect(const struct remesa_gears_geometry *g,
                                struct remesa_gears_geometry_view *out)
{
    if(!g||!out||snapshot_overlap(out,sizeof(*out),g,sizeof(*g)+g->view.bytes))return EINVAL;
    *out=g->view;return 0;
}

void remesa_gears_geometry_destroy(struct remesa_gears_geometry *g)
{ free(g); }

enum remesa_draw_status remesa_prepare_constant_draw(
    const struct pipe_draw_info *info, const struct pipe_draw_start_count_bias *draw,
    const struct pipe_vertex_buffer *vertices, const struct pipe_vertex_element *position,
    const void *vertex_snapshot, size_t vertex_bytes,
    const void *index_snapshot, size_t index_bytes, uint32_t argb,
    const struct pipe_viewport_state *viewport, uint32_t width, uint32_t height,
    struct remesa_prepared_triangles *out)
{
    struct remesa_draw_plan plan = {0};
    struct remesa_prepared_triangles *result;
    enum remesa_draw_status status;
    uint64_t base;
    unsigned fetch_bytes, active_count;
    if (!info || !draw || !vertices || !position || !viewport || !out || !width || !height)
        return REMESA_DRAW_INVALID;
    if (info->mode != MESA_PRIM_TRIANGLES || info->primitive_restart ||
        info->has_user_indices || info->take_index_buffer_ownership ||
        info->instance_count > 1 || info->start_instance || vertices->is_user_buffer ||
        position->instance_divisor || position->dual_slot || position->vertex_buffer_index ||
        !remesa_position_format_bytes(position->src_format) || !position->src_stride)
        return REMESA_DRAW_UNSUPPORTED;
    /* TRIANGLES consume complete groups of three; an incomplete final
     * primitive does not participate in any vertex/index bounds or fetch. */
    active_count = draw->count - draw->count % 3;
    if (active_count > REMESA_DRAW_MAX_INDICES) return REMESA_DRAW_UNSUPPORTED;
    if (!active_count || !info->instance_count) {
        memset(out, 0, sizeof(*out));
        out->width = width; out->height = height; out->argb = argb;
        return REMESA_DRAW_EMPTY;
    }
    if (!vertices->buffer.resource || vertices->buffer.resource->target != PIPE_BUFFER ||
        !vertex_snapshot || vertex_bytes < vertices->buffer.resource->width0)
        return REMESA_DRAW_INVALID;
    if (info->index_size) {
        status = remesa_validate_indexed_triangles(info, draw, vertices, position,
                                                  index_snapshot, index_bytes, &plan);
        if (status != REMESA_DRAW_VALIDATED && status != REMESA_DRAW_EMPTY) return status;
    } else if ((uint64_t)draw->start + active_count > (uint64_t)UINT32_MAX + 1) {
        return REMESA_DRAW_INVALID;
    }
    fetch_bytes = remesa_position_format_bytes(position->src_format);
    result = calloc(1, sizeof(*result));
    if (!result) return REMESA_DRAW_NOMEM;
    result->width = width; result->height = height; result->argb = argb;
    base = (uint64_t)vertices->buffer_offset + position->src_offset;
    status = REMESA_DRAW_VALIDATED;
    for (unsigned triangle=0; triangle < active_count; triangle+=3) {
        float clip[3][4];
        struct remesa_clipped_polygon polygon;
        for (unsigned vertex=0; vertex<3; ++vertex) {
            unsigned index = triangle + vertex;
            uint32_t source = info->index_size ?
                (uint32_t)((int64_t)read_index((const unsigned char *)index_snapshot +
                     (size_t)plan.index_first_byte + (size_t)index * info->index_size,
                     info->index_size) + draw->index_bias) : draw->start + index;
            uint64_t available = vertices->buffer.resource->width0;
            /* Subtraction validates the fetch without overflowing addition or
             * source*stride, even for malformed UINT32_MAX-sized strides. */
            if (base > available || available-base < fetch_bytes ||
                source > (available-base-fetch_bytes) / position->src_stride) {
                status = REMESA_DRAW_INVALID; goto done;
            }
            fetch_position(clip[vertex], (const unsigned char *)vertex_snapshot +
                   (size_t)(base + (uint64_t)source * position->src_stride), fetch_bytes);
        }
        int error = remesa_prepare_constant_triangle((const float (*)[4])clip,
                                                     viewport,width,height,&polygon);
        if (error) {
            status = error == EINVAL ? REMESA_DRAW_INVALID : REMESA_DRAW_UNSUPPORTED;
            goto done;
        }
        for (unsigned vertex=2; vertex < polygon.count; ++vertex) {
            if (result->vertex_count > REMESA_DRAW_MAX_INDICES-3) {
                status = REMESA_DRAW_UNSUPPORTED; goto done;
            }
            memcpy(result->positions[result->vertex_count++],polygon.window[0],16);
            memcpy(result->positions[result->vertex_count++],polygon.window[vertex-1],16);
            memcpy(result->positions[result->vertex_count++],polygon.window[vertex],16);
        }
    }
    *out = *result;
    if (!result->vertex_count) status = REMESA_DRAW_EMPTY;
done:
    free(result);
    return status;
}

enum remesa_draw_status remesa_pack_prepared_triangles(
    const struct remesa_prepared_triangles *in,struct remesa_draw_packet *out)
{
    if(!in || !out) return REMESA_DRAW_INVALID;
    uintptr_t begin=(uintptr_t)in,end=(uintptr_t)out;
    if(sizeof(*in)>UINTPTR_MAX-begin || sizeof(*out)>UINTPTR_MAX-end ||
       (begin<end+sizeof(*out) && end<begin+sizeof(*in))) return REMESA_DRAW_INVALID;
    if(in->width!=32 || in->height!=32 || in->vertex_count>REMESA_DRAW_MAX_INDICES)
        return REMESA_DRAW_UNSUPPORTED;
    if(in->vertex_count%3) return REMESA_DRAW_INVALID;
    if(!in->vertex_count) return REMESA_DRAW_EMPTY;
    struct remesa_draw_packet *packet=calloc(1,sizeof(*packet));
    if(!packet) return REMESA_DRAW_NOMEM;
    packet->width=packet->height=32;packet->argb=in->argb;
    packet->index_count=in->vertex_count;
    enum remesa_draw_status result=REMESA_DRAW_VALIDATED;
    for(unsigned i=0;i<in->vertex_count;i++) {
        unsigned vertex=0;
        while(vertex<packet->vertex_count &&
              memcmp(packet->positions[vertex],in->positions[i],sizeof(in->positions[i]))) ++vertex;
        if(vertex==packet->vertex_count) {
            if(vertex==REMESA_DRAW_MAX_VERTICES) {result=REMESA_DRAW_UNSUPPORTED;goto done;}
            memcpy(packet->positions[vertex],in->positions[i],sizeof(in->positions[i]));
            ++packet->vertex_count;
        }
        packet->indices[i]=(uint16_t)vertex;
    }
    enum remesa_packet_status status=remesa_draw_packet_validate(packet);
    if(status==REMESA_PACKET_OK) *out=*packet;
    else result=status==REMESA_PACKET_UNSUPPORTED?REMESA_DRAW_UNSUPPORTED:REMESA_DRAW_INVALID;
done:
    free(packet);return result;
}

enum remesa_draw_status remesa_validate_indexed_triangles(
    const struct pipe_draw_info *info,
    const struct pipe_draw_start_count_bias *draw,
    const struct pipe_vertex_buffer *vertices,
    const struct pipe_vertex_element *position,
    const void *index_snapshot, size_t snapshot_bytes,
    struct remesa_draw_plan *plan)
{
    struct remesa_draw_plan p = {0};
    const struct pipe_resource *ib, *vb;
    uint64_t first, end, vertex_end;
    uint32_t min = UINT32_MAX, max = 0;
    unsigned i, active_count;
    if (plan) memset(plan, 0, sizeof(*plan));
    if (!info || !draw || !vertices || !position || !plan)
        return REMESA_DRAW_INVALID;
    if (info->mode != MESA_PRIM_TRIANGLES ||
        info->primitive_restart || info->has_user_indices ||
        info->take_index_buffer_ownership || vertices->is_user_buffer ||
        info->instance_count > 1 || info->start_instance ||
        position->instance_divisor || position->dual_slot ||
        position->vertex_buffer_index ||
        !remesa_position_format_bytes(position->src_format))
        return REMESA_DRAW_UNSUPPORTED;
    active_count = draw->count - draw->count % 3;
    if (!active_count || !info->instance_count) return REMESA_DRAW_EMPTY;
    /* Legal Gallium inputs outside this first experimental packet subset. */
    if (!position->src_stride || !info->index_size)
        return REMESA_DRAW_UNSUPPORTED;
    if (info->index_size != 1 && info->index_size != 2 && info->index_size != 4)
        return REMESA_DRAW_INVALID;
    ib = info->index.resource;
    vb = vertices->buffer.resource;
    if (!ib || !vb || ib->target != PIPE_BUFFER || vb->target != PIPE_BUFFER ||
        !index_snapshot) return REMESA_DRAW_INVALID;
    first = (uint64_t)draw->start * info->index_size;
    end = first + (uint64_t)active_count * info->index_size;
    if (end > ib->width0 || end > snapshot_bytes) return REMESA_DRAW_INVALID;
    for (i = 0; i < active_count; ++i) {
        uint32_t raw = read_index((const unsigned char *)index_snapshot +
                                  (size_t)first + (size_t)i * info->index_size,
                                  info->index_size);
        int64_t biased = (int64_t)raw + draw->index_bias;
        if (info->index_bounds_valid &&
            (raw < info->min_index || raw > info->max_index))
            return REMESA_DRAW_INVALID;
        if (biased < 0 || biased > UINT32_MAX) return REMESA_DRAW_INVALID;
        if ((uint32_t)biased < min) min = (uint32_t)biased;
        if ((uint32_t)biased > max) max = (uint32_t)biased;
    }
    /* Format-width fetch only, not a complete vertex/shader/command layout. */
    unsigned fetch_bytes=remesa_position_format_bytes(position->src_format);
    uint64_t base = (uint64_t)vertices->buffer_offset + position->src_offset;
    if (base > vb->width0 || vb->width0-base < fetch_bytes ||
        max > (vb->width0-base-fetch_bytes) / position->src_stride)
        return REMESA_DRAW_INVALID;
    vertex_end = base + (uint64_t)max * position->src_stride + fetch_bytes;
    p.primitive_count = active_count / 3;
    p.min_vertex = min; p.max_vertex = max;
    p.index_first_byte = first; p.index_end_byte = end;
    p.vertex_end_byte = vertex_end;
    *plan = p;
    return REMESA_DRAW_VALIDATED;
}

enum remesa_draw_status remesa_pack_triangle_draw_vertex(
    const struct pipe_draw_info *info,
    const struct pipe_draw_start_count_bias *draw,
    const struct pipe_vertex_buffer *vertices,
    const struct pipe_vertex_element *position,
    const void *vertex_snapshot, size_t vertex_bytes,
    const void *index_snapshot, size_t index_bytes,
    uint32_t argb,const struct pipe_viewport_state *viewport, struct remesa_draw_packet *packet)
{
    struct remesa_draw_plan plan;
    float (*positions)[4] = NULL;
    uint32_t *source_ids = NULL, unique = 0, capacity;
    uint16_t *indices = NULL;
    enum remesa_draw_status status;
    enum remesa_packet_status built;
    uint64_t end;
    unsigned fetch_bytes, active_count;
    if (!info || !draw || !vertices || !position || !packet)
        return REMESA_DRAW_INVALID;
    if (viewport) {
        if (viewport->scale[0]!=16 || viewport->scale[1]!=16 || viewport->scale[2]!=.5f ||
            viewport->translate[0]!=16 || viewport->translate[1]!=16 || viewport->translate[2]!=.5f ||
            !remesa_viewport_swizzle_is_identity(viewport))
            return REMESA_DRAW_UNSUPPORTED;
    }
    if (info->mode != MESA_PRIM_TRIANGLES || info->primitive_restart ||
        info->instance_count > 1 || info->start_instance || vertices->is_user_buffer ||
        position->instance_divisor || position->dual_slot || position->vertex_buffer_index ||
        !remesa_position_format_bytes(position->src_format) || !position->src_stride)
        return REMESA_DRAW_UNSUPPORTED;
    active_count = draw->count - draw->count % 3;
    if (!active_count || !info->instance_count) return REMESA_DRAW_EMPTY;
    fetch_bytes=remesa_position_format_bytes(position->src_format);
    if (active_count > REMESA_DRAW_MAX_INDICES)
        return REMESA_DRAW_UNSUPPORTED;
    if (!vertices->buffer.resource || vertices->buffer.resource->target != PIPE_BUFFER ||
        !vertex_snapshot || vertex_bytes < vertices->buffer.resource->width0)
        return REMESA_DRAW_INVALID;
    if (info->index_size) {
        status = remesa_validate_indexed_triangles(info, draw, vertices, position,
                                                  index_snapshot, index_bytes, &plan);
        if (status != REMESA_DRAW_VALIDATED) return status;
    } else {
        /* The index union and index_bias are unspecified for this draw kind. */
        end = (uint64_t)draw->start + active_count;
        if (end > (uint64_t)UINT32_MAX + 1) return REMESA_DRAW_INVALID;
        uint64_t base = (uint64_t)vertices->buffer_offset + position->src_offset;
        uint64_t available = vertices->buffer.resource->width0;
        if (base > available || available-base < fetch_bytes ||
            end-1 > (available-base-fetch_bytes) / position->src_stride)
            return REMESA_DRAW_INVALID;
    }
    capacity = active_count < REMESA_DRAW_MAX_VERTICES ? active_count : REMESA_DRAW_MAX_VERTICES;
    positions = calloc(capacity, sizeof(*positions));
    source_ids = malloc(capacity * sizeof(*source_ids));
    indices = malloc(active_count * sizeof(*indices));
    status = REMESA_DRAW_NOMEM;
    if (!positions || !source_ids || !indices) goto done;
    for (unsigned i = 0; i < active_count; ++i) {
        uint32_t source = info->index_size ?
            (uint32_t)((int64_t)read_index((const unsigned char *)index_snapshot +
                (size_t)plan.index_first_byte + (size_t)i * info->index_size,
                info->index_size) + draw->index_bias) : draw->start + i;
        unsigned compact = 0;
        while (compact < unique && source_ids[compact] != source) ++compact;
        if (compact == unique) {
            if (unique == capacity) { status = REMESA_DRAW_UNSUPPORTED; goto done; }
            source_ids[unique] = source;
            fetch_position(positions[unique], (const unsigned char *)vertex_snapshot +
                   (size_t)((uint64_t)vertices->buffer_offset + position->src_offset +
                            (uint64_t)source * position->src_stride), fetch_bytes);
            if (viewport) {
                /* Identity VS semantics are host preparation, never CPU raster.
                 * All vertices lie inside canonical clip volume: no edges need
                 * clipping; w=1 avoids unqualified perspective interpolation. */
                for(unsigned axis=0;axis<4;axis++)
                    if(!isfinite(positions[unique][axis])) { status=REMESA_DRAW_INVALID;goto done; }
                if(positions[unique][3]!=1) { status=REMESA_DRAW_UNSUPPORTED;goto done; }
                for(unsigned axis=0;axis<3;axis++) {
                    if(positions[unique][axis]<-1 || positions[unique][axis]>1) { status=REMESA_DRAW_UNSUPPORTED;goto done; }
                    positions[unique][axis]=positions[unique][axis]*viewport->scale[axis]+viewport->translate[axis];
                }
            }
            ++unique;
        }
        indices[i] = (uint16_t)compact;
    }
    built = remesa_draw_packet_build(packet, (const float (*)[4])positions,
                                    unique, indices, active_count, argb);
    status = built == REMESA_PACKET_OK ? REMESA_DRAW_VALIDATED :
             built == REMESA_PACKET_UNSUPPORTED ? REMESA_DRAW_UNSUPPORTED : REMESA_DRAW_INVALID;
done:
    free(positions); free(source_ids); free(indices);
    return status;
}

enum remesa_draw_status remesa_pack_triangle_draw(
 const struct pipe_draw_info *info,const struct pipe_draw_start_count_bias *draw,
 const struct pipe_vertex_buffer *vertices,const struct pipe_vertex_element *position,
 const void *vertex_snapshot,size_t vertex_bytes,const void *index_snapshot,size_t index_bytes,
 uint32_t argb,struct remesa_draw_packet *packet)
{
 return remesa_pack_triangle_draw_vertex(info,draw,vertices,position,vertex_snapshot,vertex_bytes,
                                        index_snapshot,index_bytes,argb,NULL,packet);
}

struct remesa_linked_vertex_inputs {
 struct remesa_linked_vertex_inputs_view view;
 size_t storage_bytes;
 unsigned char data[];
};
int remesa_linked_vertex_inputs_create(const struct pipe_draw_info *info,
 const struct pipe_draw_start_count_bias *draw,const struct remesa_linked_attribute attributes[3],
 const void *indices,size_t index_bytes,struct remesa_linked_vertex_inputs **out)
{
 if(!info||!draw||!attributes||!out||
    snapshot_overlap(out,sizeof(*out),info,sizeof(*info))||
    snapshot_overlap(out,sizeof(*out),draw,sizeof(*draw))||
    snapshot_overlap(out,sizeof(*out),attributes,3*sizeof(*attributes)))return EINVAL;
 const struct pipe_draw_info in=*info;
 const struct pipe_draw_start_count_bias d=*draw;
 struct remesa_linked_attribute a[3];memcpy(a,attributes,sizeof(a));
 for(unsigned k=0;k<3;k++){
  if(snapshot_overlap(out,sizeof(*out),a[k].data,a[k].bytes))return EINVAL;
  if(a[k].format!=PIPE_FORMAT_R32G32B32A32_FLOAT)return ENOTSUP;
  if(!a[k].data||a[k].bytes>UINTPTR_MAX-(uintptr_t)a[k].data||
     a[k].offset>a[k].bytes||16>a[k].bytes-a[k].offset)return EINVAL;
 }
 if(in.index_size&&snapshot_overlap(out,sizeof(*out),indices,index_bytes))return EINVAL;
 if(*out)return EBUSY;
 if(in.mode!=MESA_PRIM_TRIANGLES||in.start_instance||in.instance_count>1||
    (in.index_size&&in.primitive_restart))return ENOTSUP;
 const uint32_t count=d.count-d.count%3;
 if(!count||!in.instance_count)return ENODATA;
 if(count>65535)return E2BIG;
 uint32_t min=UINT32_MAX,max=0;uint64_t first=0;
 if(in.index_size){
  if(in.index_size!=1&&in.index_size!=2&&in.index_size!=4)return EINVAL;
  if(!indices||index_bytes>UINTPTR_MAX-(uintptr_t)indices||
     snapshot_overlap(out,sizeof(*out),indices,index_bytes))return EINVAL;
  first=(uint64_t)d.start*in.index_size;
  if(first>index_bytes||(uint64_t)count*in.index_size>index_bytes-first)return EINVAL;
  for(uint32_t i=0;i<count;i++){
   uint32_t raw=read_index((const unsigned char *)indices+(size_t)first+(size_t)i*in.index_size,in.index_size);
   if(in.index_bounds_valid&&(raw<in.min_index||raw>in.max_index))return EINVAL;
   int64_t value=(int64_t)raw+d.index_bias;
   if(value<0||value>UINT32_MAX)return EINVAL;
   if((uint32_t)value<min)min=(uint32_t)value;
   if((uint32_t)value>max)max=(uint32_t)value;
  }
 }else{
  uint64_t end=(uint64_t)d.start+count;
  if(end>(uint64_t)UINT32_MAX+1)return EINVAL;
  min=d.start;max=(uint32_t)(end-1);
 }
 uint64_t records=(uint64_t)max-min+1;
 if(records>4095)return E2BIG;
 for(unsigned k=0;k<3;k++)if(a[k].stride&&max>(a[k].bytes-(size_t)a[k].offset-16)/a[k].stride)return EINVAL;
 const size_t vb=(size_t)records*48,ib=(size_t)count*2;
 struct remesa_linked_vertex_inputs *owned=malloc(sizeof(*owned)+vb+ib);
 if(!owned)return ENOMEM;
 *owned=(struct remesa_linked_vertex_inputs){.storage_bytes=vb+ib,
  .view={.vertices=owned->data,.indices=(uint16_t *)(owned->data+vb),
   .vertex_bytes=vb,.index_bytes=ib,.vertex_count=(uint32_t)records,.index_count=count,
   .stride_bytes=48,.min_source_vertex=min,.max_source_vertex=max}};
 for(uint32_t v=0;v<(uint32_t)records;v++)for(unsigned k=0;k<3;k++)
  memcpy(owned->data+(size_t)v*48+k*16,(const unsigned char *)a[k].data+
   (size_t)a[k].offset+(size_t)(min+v)*a[k].stride,16);
 uint16_t *rebased=(uint16_t *)(owned->data+vb);
 for(uint32_t i=0;i<count;i++){
  uint32_t id=in.index_size?(uint32_t)((int64_t)read_index((const unsigned char *)indices+
   (size_t)first+(size_t)i*in.index_size,in.index_size)+d.index_bias):d.start+i;
  rebased[i]=(uint16_t)(id-min);
 }
 *out=owned;return 0;
}
int remesa_linked_vertex_inputs_inspect(const struct remesa_linked_vertex_inputs *owned,
 struct remesa_linked_vertex_inputs_view *out)
{
 if(!owned||!out||snapshot_overlap(out,sizeof(*out),owned,sizeof(*owned)+owned->storage_bytes))return EINVAL;
 *out=owned->view;return 0;
}
void remesa_linked_vertex_inputs_destroy(struct remesa_linked_vertex_inputs *owned)
{free(owned);}
