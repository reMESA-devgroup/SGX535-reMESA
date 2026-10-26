# Sprint — PDS task output consumer

Implementação incremental sobre `phase9-execution-contract-20261018`, no worktree
`driver-resume-20261018`. O checkpoint anterior teve seus 1.252 artefatos selados
e 262 fontes/documentos conferidos antes das alterações. O diretório desta
entrega é `/home/gama/sgx535-offline/phase9-task-output-consumer-20261018`; o nome
conserva a data de início da sprint, concluída em 19/10/2026.

O avanço funcional é gerar e consumir um grupo CPU pixel completo a partir do
par NIR/VS/FS e do vínculo USE atual, sem exigir uma receita FS manual com campos
superiores opacos. O perfil tem codec original, receita DATA tipada, armazenamento
imutável, validação dos três valores/fontes e integração Gallium. Não foi
encontrado um decoder do consumidor físico. CODE PDS executável permanece
`ENOTSUP`; despacho, término e bancos DS físicos continuam UNKNOWN.

## Evidência e seu alcance

`evidence/XPSB-PIXEL-CPU-FIELD-MATRIX.json` contém a matriz por campo, loci,
máscaras, hashes e limitações. Os fatos são operações da CPU, não execução GPU.

| Campo | Regra observada | Estado e limite |
|---|---|---|
| Perfil | `Xpsb_emit_pixel_shader`, zero descritores de textura; prefixo de 12 DWORDs | CONFIRMED no produtor CPU; não generalizar para VS/texturas |
| Grupo CPU | Valores em DATA+0, +4, +32 | CONFIRMED; três valores inicializados não provam aridade de task output |
| T0 baixo | `bank \| (((offset>>15)&15)<<4) \| (((offset>>4)&2047)<<8)` | CONFIRMED no contrato de relocation já implementado |
| T0 superior | `T0 & ~0x7ffff == 0x00100000` | CONFIRMED para este perfil; significado arquitetural desconhecido |
| T1 | `uint32(metadata6 << 27)`, domínio observado 0/1 | CONFIRMED no writer CPU; não inferir metadata6 da cor ou tamanho USSE |
| T2 | literal `0x20` | CONFIRMED no produtor CPU; não chamar de recurso, enable ou END |
| Outras células | Não inicializadas pelo caminho examinado | CONFIRMED nessa CPU; seu conteúdo histórico e consumo GPU são UNKNOWN |
| DS físico/preload | Banco, índice, momento e mecanismo de transferência | UNKNOWN |
| Aridade/porta | Fontes realmente lidas e interpretação de controles | UNKNOWN |
| Despacho/término | Issue, drain, fim PDS/USSE e sinalização | UNKNOWN |

A relação nova fecha o background de T0: ELF `0x890d` passa `0x00100000` ao
helper de relocation. `XpsbSetUSERegRelocation` o guarda no record+0x18. O
consumidor GPL histórico `psb_sgx.c:754..770` aplica esse background à operação
5, enquanto as duas operações 4 usam o destino/cache acumulado como background.
Suas máscaras disjuntas são `0x0000000f`, `0x000000f0` e `0x0007ff00`. Assim os
bits superiores são exatos neste perfil, não controles arbitrários admitidos.
A família ABI histórica não é autenticada como o kernel efetivamente pareado
com uma captura específica; a composição é uma conclusão estática de software.

As evidências REA novas analisam somente os três helpers CPU de relocation,
com SHA256 do Xpsb e Evidence IDs nos JSONs `evidence/REA-*.json`. Ghidra importa
esse ELF com base adicional `0x10000`: a função ELF `0x8d90` é `0x18d90` nessa
sessão. Nenhuma sequência shader foi extraída. A verificação independente
confere 32 âncoras x86, cinco membros do archive GPL e 524.288 composições
aritméticas dos campos; isso não constitui teste do hardware.

Os campos lower19, metadata6 e posições DATA já eram conhecidos; a ligação
exata do background até seu consumidor e sua implementação como perfil completo
são desta sprint. Não foi repetida a cadeia CPU→MMIO nem a investigação do PC.

## Implementação

**IMPLEMENTED_OFFLINE — `remesa_pds.c/.h`.**

- `cpu_pixel_task_pack/unpack`: compõem e verificam os três valores CPU.
  Reutilizam o plano USE e o codec metadata6. Exigem data master pixel, entrada
  representável sem perda e background/companheiros exatos; rejeitam bits fora
  do perfil e preservam outputs em erros.
- `cpu_pixel_task_recipe`: constrói 12 células tipadas. Célula0 recebe
  USE lower19 com background `0x00100000`; célula1 recebe CPU_META6; célula8
  recebe `0x20`. As nove células restantes são zero por política host original,
  sem afirmar que eram zero ou não eram lidas historicamente.
- `cpu_pixel_task_prepare`: consome os valores dos planos lógicos existentes,
  verifica estágio, link, endereço, máscaras, tipos, padding host, destinos
  únicos e posições exatas. Retorna três fontes com célula, slot lógico, valor
  e endereço DATA. Slot lógico não é DS físico. Os erros de DATA→DS físico,
  despacho e término permanecem ENOTSUP, inclusive após sucesso desta função.

**IMPLEMENTED_OFFLINE — pipeline e Gallium.**

`remesa_shader_host_pipeline_create_pixel_task()` deriva a receita FS e chama
o construtor imutável existente. Reutiliza armazenamento, relocations, par de
shaders, mapeamento VS e receita VS. Requer o corpo FS original restrito de oito
bytes, DATA FS de 48 bytes e relocation em DATA+0. `request.data[1]` é ignorada
explicitamente nesta API: o chamador não precisa fornecê-la. O constructor
copia a nova receita; nenhum ponteiro para a pilha é retido. A seleção de
metadata6 é explícita e sua adequação física ao novo shader permanece UNKNOWN.

`revalidate_pixel_task()` reconstrói a mesma receita a partir das entradas
atuais. `prepare_pixel_task_output()` usa o contrato completo VS/FS já validado
e o armazenamento DATA real para produzir o grupo CPU. `validate_pixel_task_output()`
reconstrói e compara todos os campos, incluindo fontes, endereços e bloqueios.

Os três wrappers `remesa_context_*pixel_task*` validam o par atualmente vinculado
e o layout atual antes de preparar/revalidar a saída. Preservam o erro Gallium,
os bindings e os diagnósticos anteriores; falha de compilação não substitui o
par vinculado. Rebind, metadata6 diferente e mapping generation stale são
detectados. Não foi alterado o backend Linux nem habilitado um default novo.

O teste existente de `draw_vbo` agora constrói e verifica também este perfil
no callback do backend de gravação. Mantém os casos anteriores de erro de
receita, framebuffer, recursos, fences e preservação de estado. Os quatro
checks novos levam essa suíte de 68 para 72, sem remover casos anteriores.
O backend de gravação verifica somente contratos CPU; não simula prova de GPU.

## Validação e artefatos

`RESULTS.json` vincula as matrizes finais e os logs. Foram executados:

- codec/perfil pixel nativo, UBSan e i386/QEMU, cobrindo todos os bancos e
  offsets representáveis, ambos metadata6 e controles/máscaras malformados;
- integração do perfil com NIR, armazenamento, relocations, pipeline e Gallium,
  com 340 checks por variante, incluindo overflow, aliases, estado incompleto,
  imutabilidade, rollback de alocação e rebind;
- regressões anteriores de task planning, VS export, USSE/PDS, DATA, pipeline,
  corpo FS, programas, NIR/draw/caps, recursos, framebuffer e sincronização;
- arquivo estático e DRI Mesa 25.2.0 i386, sem instalação; 18 jobs contra o
  arquivo real, com fontes staged iguais aos do worktree e Ninja sem pendências.

O perfil pixel é testado com verde e magenta, metadata6=0 e 1. Isso testa a
aritmética e a integração CPU dessas entradas, não sua validade de execução.
`examples/` contém as quatro DATA de 48 bytes geradas a partir do armazenamento
imutável real. Os candidatos USSE anteriores continuam byte a byte iguais.
Esses arquivos DATA não são programas PDS executáveis nem recebem CODE copiado.

`implementation.patch` é incremental sobre `before/`, não sobre o HEAD Git já
modificado do worktree. `PATCH-ROUNDTRIP.json` verifica sua aplicação e a
reconstrução exata dos fontes. `REPRODUCE.py` repete as compilações e testes com
dependências locais. `MANIFEST.json` sela os artefatos; o checkpoint anterior,
seus históricos e os binários Mesa anteriores foram preservados.

## Menor bloqueio restante

**BLOCKED:** falta a regra física de task output que liga as fontes DATA/DS à
porta USSE: seleção de bancos/índices, quantidade de fontes consumidas e
interpretação dos controles, junto de uma regra de término. Os três valores
CPU agora podem ser gerados e verificados sem receita FS manual; eles ainda
não fornecem essa regra. Não há evidência suficiente para um encoder CODE PDS
mínimo executável. `remesa_pds_encode()` e a emissão final do contrato continuam
ENOTSUP exatamente nessa fronteira. Os perfis USSE/NIR não suportados seguem
com rejeições próprias; o VS físico/exportação também continua bloqueado.

O próximo passo mínimo é obter um decoder/consumidor independente aplicável à
SGX535 que estabeleça as fontes físicas e o task-output control desse grupo.
Ele poderá alimentar o lowering a partir dos valores, relocations e fontes
agora prontos, sem reescrever armazenamento ou Gallium. Um readback isolado,
timeout ou travamento não substitui essa evidência.

Nenhum opcode, banco físico, flag de término ou aridade foi inventado. Zero
DDK vazado, cópia de programas proprietários, submissões GPU e conexões SSH.
ADMISSION, gates e defaults operacionais estão intactos. Primeiro PC permanece
UNKNOWN, com B+q como hipótese operacional anterior e investigação inativa.
Não se declara contexto OpenGL 2.1 funcional.
