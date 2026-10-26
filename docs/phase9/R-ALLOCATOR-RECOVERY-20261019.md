# R allocator recovery — implementação original, CPU/offline

**R_ALLOCATOR_RECOVERY_PASS**, no subconjunto descrito abaixo. Investigação executada em20/10/2026 no diretório solicitado `r-allocator-recovery-20261019`. Este estado confirma regras CPU novas implementadas e verificadas; programa PDS completo e execução GPU continuam **ENOTSUP**.

## O que foi fechado

Em `psb_dri.so` SHA256 `74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8`, Ghidra12.1.4 confirmou as funções `FUN_001d3ba5` (inicialização), `FUN_00247b63` (alocação) e `FUN_002452a0` (seleção). Endereços abaixo são ELF/file; Ghidra=ELF+10000.

| Campo/etapa | Regra CONFIRMED | Evidência primária |
|---|---|---|
| ctx+34 | quantidade de descritores; controla percursos, append e remoção | 1c3d2a;207f93..208007;235c69..235c93;238c41 |
| ctx+38 | ponteiro do array; registros32B | 1c3c71/1c3c80;235c72/235c7a |
| Inicialização | 14 R tipo11, IDs0..13, components4/mode0/width4; capacidade31 registros | 1c3ba5..1c3d31; alloc3e0 |
| ctx+90 inicial | extent lógico56, distinto do tamanho DATA PDS | 1c3d31 |
| Liveness | slot k vivo se nibble `(mask[k/8]>>(4*(k%8)))&15` não zero | bitset EBP-5c;2383a4..2383a7;238412..238530 |
| R genérico mode0 | elimina cauda morta; mantém prefixo até último slot vivo, com buracos interiores | 2386fd..238b24;238bdf..238c2f;replays swizzle |
| R10/R11 e não-R | elimina entrada se nenhum slot vivo; caso contrário mantém largura inteira | 2383e4..2385ae |
| Compactação | remove registros sem reordenar sobreviventes; oldbase avança pela largura ANTIGA | 235c52;23853b/238b46;238c46 |
| Ordenação | primeiro R, depois primeiro não-R, alternando; fallback quando grupo esgota; seleção estável | 2352a0;2391ee..239257;memmove2393ac |
| Consumidor | mapa antigo→novo reescreve índice das fontes internas file2 | 239653..2396af, `MOV [EDX+4],EDI` |
| Entrada PDS | wrapper comprime32B→8B mantendo a sequência; encoder25970 atribui outros índices a/b/j | 353af..354b0;26345..263c4 |

**Demonstração completa original:** `R0..R13,T0 → R10,R11,T0 → R10,T0,R11`. A poda termina em238d60. Depois, as seleções `(i,j)=(0,0),(1,2),(2,2)` movem T para posição1 em2393ac. Slots antigos R10=40..43,R11=44..47,T=56 tornam-se0..3,5..8,4; extent final9. Só então o construtor PDS funde R10/T e emite seus A2/classe64 calculados. Esses slots do compilador NÃO são índices DS físicos nem células DATA.

## Diferenciais que distinguem hipóteses

Nove programas próprios semanticamente válidos passaram pelo frontend/compilador/linker/construtor x86 original em Unicorn, com serviços CPU explicitamente admitidos. Nenhuma lista interna foi injetada. Nove verificações independentes por cabeçalhos públicos Mesa7.4 compilados para i386 coincidem:2176 bytes de IR. As instruções USE geradas ficam privadas no emulador, sem exportação/reutilização.

| Variação válida | Resultado original |
|---|---|
| ADD antes/depois do TEX; operandos de cor invertidos | R10,T0,R11; ordem textual não determina a lista final |
| Dois atributos genéricos, outra coordenada T | R0,T2,R1 |
| Três R antes de uma T | R1,T0,R10,R11; fallback preserva ordem dos R restantes |
| Duas T, sem R vivo | T0,T1; fallback sem inverter T |
| R0.xyzw | width4, P5, T antigo56→novo4 |
| R0.xxxx | width1, P2, T56→1 |
| R0.zzzz | width3, P4, R antigo2→novo2 e T56→3 |

**REFUTED:** compactar por popcount dos componentes vivos (`zzzz` usa3 slots, não1); atribuir R,T,R à compactação simples; inferir a lista final diretamente da ordem textual do shader. A ordem dos grupos vem de seleção explícita posterior.

O inventário dos72 arquivos Windows também trouxe evidência útil: `igdumd32.dll` SHA83873d2196c5d3ed2a1735d2c60d69e6b8242942ee4c16c4b84c9d70f2408fc9 contém inicializador14R em PE VA100aef00, alocador em100ed960 e inserção estável em100ed3e0. Foram conferidos2880 bytes CPU diretamente no PE. Assertions apontam para `regpack.c`/`uNrTextureLoads`. Há correspondência estrutural com Linux, inclusive exceção de prefixo9; a possível linhagem comum impede tratar isso como validação independente do hardware. Os demais componentes receberam metadados/hashes; `.vp/.cpa` agrupados pelo INF em OPM não foram decodificados como shaders nem presumidos alocadores.

## IMPLEMENTED_OFFLINE

`remesa_pds_structure_allocate()` recebe IR R/T existente, larguras lógicas e liveness explicitamente estabelecida. Implementa poda, trim mode0, alternância estável, remapeamento com NO_CELL, ligação ao encoder estrutural existente e publicação transacional. Deriva a imagem DATA/CODE e mantém relocations simbólicas/little-endian/máscaras desconhecidas. Não contém tabela de replays nem bytecode histórico como implementação.

O limite16 entradas/64 slots é política conservadora do novo API, não limite arquitetural; o alocador original inicializa128 entradas de mapa. Flags/prefixo fixado e Rmode1/2/3 retornam ENOTSUP. `extra_r` continua sendo append delegado do encoder PDS; o mapa retornado não reconstrói as reservas USE do caminho de feature do compilador.

Integração efetiva: teste Gallium fornece `allocation.request` ao caminho existente `remesa_context_prepare_pds_structure_pipeline`, com VS/FS NIR vinculados, armazenamento imutável, relocations e revalidação. Verifica cópia/ownership, stale state e rollback sem reescrever subsistemas. Não acrescenta suporte NIR a amostragem; o FS vinculado continua sendo constante. Não habilita o caminho automaticamente.

## Validação executada

- 113 verificações do alocador por variante: nativo,UBSan,i386/QEMU e arquivo Mesa real.
- 1458 verificações do pipeline por variante (baseline1396 preservadas +62 novas), incluindo Gallium/ownership/ENOTSUP.
- 115 regressões DATA/classe64 por variante;377452 regressões USSE/PDS por variante nativa/UBSan/i386.
- 20 oráculos históricos em cada uma dessas3 variantes:588 bytes DATA e304 CODE conhecido coincidentes; lacunas/endereço simbólico excluídos.
- 24 comparações independentes novas de alocador/mapa/DATA/CODE:6 oráculos originais ×4 ambientes. A entrada usa registros PRÉ-poda+liveness/caller/fixup; ordem final,mapa final e CODE original são apenas oráculos.
- Mesa25.2.0 arquivo estático e DRI compilados. Testes vinculados ao arquivo real:113 alocador,115 classe64,1458 pipeline,340 pixel,207 host,32 NIR. Estabilidade Ninja real:891 artefatos compilados, nenhum hash alterado na segunda execução. DRI não instalado.

`TEST_RESULTS.json` aponta os logs/argv/hashes reais. O warning de LOAD RWX do DRI já existia no checkpoint; não foram modificadas flags de linker. O recibo inicial em `impl/` contém90 checks anteriores à expansão; a validação final é `unit/RESULTS.json` (113), além do arquivo Mesa real.

## Limites e próximo passo concreto

**CONFIRMED:** regras CPU acima, caminhos x86 originais e metadados de saída. **IMPLEMENTED_OFFLINE:** alocador mode0 e integração estrutural. **INFERRED:** homologia dos alocadores Windows/Linux; motivos de otimização da alternância. **UNKNOWN:** entrega física de atributos, operações físicas de A2/classe64,07000345/AF e first_pc. **BLOCKED/ENOTSUP:** encoder PDS completo e submissão GPU; liveness automática e modos compactados não implementados no novo API.

Próxima dependência mínima: Ghidra **247d55**, `CALL 1eaae9`, na `FUN_00247b63`. Determinar a máscara devolvida por fonte/instrução permite substituir a entrada explícita de liveness por análise original do IR. O experimento útil é acompanhar esse retorno nos três shaders xyzw/xxxx/zzzz já válidos; dispensa nova execução GPU. Ver `GHIDRA_HANDOFF.md`.

ADMISSION/gates preservados. Zero SSH,MMIO,kicks,submissões,módulos,alterações de kernel/sudo ou execução física no Mini12. Os823 artefatos do checkpoint anterior foram conferidos antes/depois. Nenhuma investigação do primeiro PC foi reaberta.
