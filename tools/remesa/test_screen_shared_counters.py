#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU pthread regression of exact screen counter helpers; reduced owner fixture.
Does not establish thread safety of Gallium, resources or backend operations.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def function(source, name):
    begin = source.index("static ", source.index(name)-20)
    brace = source.index("{", source.index(name, begin))
    depth = 1
    end = brace+1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    screen = Path(__file__).resolve().parents[2] / "mesa/sgx535/remesa_screen.c"
    source = screen.read_text()
    fixture = '''#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
/* Reduced owner; actual hold/drop/cookie functions follow. */
struct screen { unsigned refs;uint64_t resource_cookie;int device;
 int (*release_device)(int *);bool release_pending;int release_error; };
'''+"\n".join(function(source, n) for n in
             ("screen_hold", "screen_drop", "screen_next_resource_cookie"))+'''
enum {THREADS=4,ITERATIONS=50000};
static struct screen *owner;
static uint64_t values[THREADS*ITERATIONS];
static unsigned releases;
static int release(int *device){(void)device;releases++;return 0;}
static void *worker(void *argument) {
 unsigned index=(unsigned)(uintptr_t)argument;
 for(unsigned i=0;i<ITERATIONS;i++) {
  screen_hold(owner);
  assert(screen_next_resource_cookie(owner,&values[index*ITERATIONS+i]));
  screen_drop(owner);
 }
 return NULL;
}
int main(void) {
 pthread_t threads[THREADS];owner=calloc(1,sizeof(*owner));assert(owner);
 owner->refs=1;owner->release_device=release;
 for(unsigned i=0;i<THREADS;i++)assert(!pthread_create(&threads[i],NULL,worker,(void*)(uintptr_t)i));
 for(unsigned i=0;i<THREADS;i++)assert(!pthread_join(threads[i],NULL));
 assert(owner->refs==1&&owner->resource_cookie==THREADS*ITERATIONS&&!releases);
 unsigned char *seen=calloc(THREADS*ITERATIONS+1,1);assert(seen);
 for(unsigned i=0;i<THREADS*ITERATIONS;i++) {
  assert(values[i]&&values[i]<=THREADS*ITERATIONS&&!seen[values[i]]);
  seen[values[i]]=1;
 }
 free(seen);screen_drop(owner);assert(releases==1);
 struct screen limit={.resource_cookie=UINT64_MAX-1};uint64_t cookie=123;
 assert(screen_next_resource_cookie(&limit,&cookie)&&cookie==UINT64_MAX);
 cookie=123;assert(!screen_next_resource_cookie(&limit,&cookie)&&cookie==123);
 puts("PASS exact hold/drop/cookie helpers: 4 concurrent threads,200000 unique cookies,final release once,saturation; CPU fixture only");
}
'''
    cfile = output / "counters.c"
    cfile.write_text(fixture)
    rows = []
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (768*1024**2,)*2)
    for name, flags in (("native", ["-O2"]), ("ubsan", ["-O1", "-fsanitize=undefined", "-fno-sanitize-recover=all"])):
        executable = output / name
        command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                   str(cfile), "-pthread", "-latomic", "-o", str(executable)]
        for label, argv in ((name+"-build", command), (name+"-run", [str(executable)])):
            result = subprocess.run(argv, capture_output=True, text=True, timeout=60, preexec_fn=limits)
            (output / (label+".log")).write_text(result.stdout+result.stderr)
            rows.append(dict(label=label, argv=argv, exit=result.returncode))
            if result.returncode or result.stderr:raise RuntimeError(label+": "+result.stderr)
    report = dict(status="EXACT_SCREEN_COUNTER_HELPERS_PASS", GPU_operations=0,
                  scope="Reduced owner, real helpers; not full backend/resource concurrency",
                  source_sha256=hashlib.sha256(source.encode()).hexdigest(), commands=rows)
    (output / "RESULT.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(status=report["status"], GPU_operations=0)))


if __name__ == "__main__":main()
