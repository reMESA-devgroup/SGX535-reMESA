# PDS A2: replay original e encoder estrutural

**CPU_DESCRIPTOR_ENCODING_CONFIRMED: sim, no subconjunto abaixo.**
**PDS_CODE_STRUCTURE_CONFIRMED: sim, para a gramática CPU de 0x25970.**
**GPU_EXECUTION_SEMANTICS_VERIFIED: não.** O caminho de execução mantém ENOTSUP.

Evidências e reprodução: `/home/gama/sgx535-offline/pds-a2-close-20261019/`.
Os 42 artefatos e três inputs do manifest anterior foram verificados antes do
trabalho. `psb_dri.so` SHA-256:
`74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8`.

## Experimento efetivamente executado

Unicorn 2.1.4 executou as instruções **x86 originais**, desde a entrada
`0x25970` até seu retorno, incluindo os construtores originais de relocation
`0x37854`, `0x3a82c`, `0x3773b` e `0x37621`. Foram interceptados apenas
alocação, commit e três serviços de bookkeeping BO. Endereços, índices de BO e
pool offsets desses serviços são sintéticos; os registros de relocation são
escritos pelo x86 original. Nenhuma biblioteca alvo foi carregada pelo host;
o emulador não oferece dispositivos ou syscalls e restringe os PCs permitidos.

| Captura, mesma entrada T | A: link+70=0 | B: link+70=1 |
|---|---|---|
| Única diferença de input | byte em 0x50003070 | mesmo byte, valor 1 |
| DATA / início CPU de CODE | 12 DWORDs / +0x30 | iguais |
| DATA[9], último T | 0x0c00f904 | preservado |
| DATA[11] | não escrito | 0x0f80e00f, R extra |
| A2: índice j / DWORD | 1 / 0x070418a2 | mais 3 / 0x070438a2 |
| Background no registro USE +0x18 | 0x100000 | 0x180000 |
| Primeiro DWORD observado | 0x07000345 | preservado |
| CODE observado, quantidade | 4 DWORDs | 5 DWORDs |

DATA/CODE antes e depois, todos os stores, PCs, índices lidos antes dos shifts,
argumentos dos helpers e registros completos estão em `reproduced/replay/`.
Os bytes não escritos foram inicializados com CC pelo ensaio; não representam
padding histórico. O endereço sintético de CPU não é um endereço GPU.

**Descoberta adicional CONFIRMED CPU:** o produtor real `0x370d8` grava
link+70=0/3/5 (`0x3713d/0x3714d/0x37156`), e soma esse campo a +6f em +71.
Não é originalmente um booleano. Replays separados com 3 e 5 produziram a
mesma alocação e relocations que 1 em 0x25970. Isso valida o predicado desse
consumidor; o produtor completo também muda +71 e o USE, portanto não prova
equivalência de tarefas reais. A origem em preparação alpha/kill/depth é
**INFERRED** por correspondência ABI; ver `fields/OWNER70-PRODUCER.md`.
O ramo que produz 5 contém advertência de testes stencil não implementados;
existir esse estado no CPU não demonstra uma tarefa funcional.

## DATA → índice → A2, sem renumeração

O índice j salvo em `[ebp+4*i-0x124]` vem de `0x25b86` (RT), `0x25d9c` (R),
`0x25ea6` (T) ou `0x260be` (R extra). O DWORD DATA fica no offset CPU
`4*((j&7)+8+16*(j>>3))`. O mesmo array alimenta os marcadores e o load
`0x26345`; `0x2634e–0x26362` transforma j e `0x2636a` escreve CODE.
O valor composto de DATA não é inserido no A2. A leitura física pela GPU
desse DWORD, de fontes adicionais ou implícitas continua **UNKNOWN**.

```text
A2(j) = uint32(0x070410a2 | ((j&1)<<11) | ((j>>1)<<13))
```

| Bits, para j<64 | Expressão produtora | Dependência / evidência | Confiança |
|---|---|---|---|
| 31:27 | 0 | OR 0x26353 e shifts | CONFIRMED CPU |
| 26:24 | 111 | OR 0x26353 | CONFIRMED CPU |
| 23:19 | 0 | domínio limitado de j | CONFIRMED CPU |
| 18 | 1 | OR 0x26353 | CONFIRMED CPU |
| 17:13 | j[5:1] | SHR 0x2634e, SHL 0x26350 | CONFIRMED CPU |
| 12 | 1 | (j&1)+2, SHL 0x2635f | CONFIRMED CPU |
| 11 | j[0] | AND/ADD/SHL 0x26359–0x2635f | CONFIRMED CPU |
| 10:8 | 0 | expressão completa | CONFIRMED CPU |
| 7:0 | 0xa2 | OR 0x26353 | CONFIRMED CPU |

Esses bits constantes não foram identificados como campos reservados ou
opcodes físicos. Banco/metade/modo/porto e aridade são **UNKNOWN**. Bit11 de
CODE é paridade de j; bit11 de DATA é outro campo, marcado no último T.
O x86 não mascara j a seis bits: um replay separado de seu bloco
`0x26345–0x2636b`, com operandos injetados, confirmou alias j=0/j=64 e mudança
para j=128. O limite j<64 do encoder é política de subconjunto, não ISA.

Fusão ocorre só para R imediatamente seguido por T; economiza exatamente um
A2 e preserva o DWORD 64 de T. T→R, RR, TT e R extra não fundem. Listas com
0, 1, 2 e 3 entradas foram executadas. T sozinho e RT fundido geraram CODE
idêntico, distinguindo-se em DATA e background. Alterar id_R 2→3 ou id_T 4→5
mudou DATA, preservando CODE. A notação “A2 +64” significa outro DWORD da
classe hexadecimal 0x64, não uma soma de 64 nem renumeração do índice.

## Implementação e validação

Novos arquivos: `mesa/sgx535/remesa_pds_structure.h/.c` e
`mesa/sgx535/tests/test_pds_structure.c`. Implementam DATA R/T simbólico,
fusão, layout intercalado, marcadores, R extra por decomposição aritmética do
DATA observado, fixups simbólicos, extensão e DWORDs A2/64 calculados.
Não incorporam sequências históricas de shader. A decomposição do R extra
não atribui significado físico a seus campos.

Os DWORDs inicial/final são **holes com máscara conhecida zero**. DATA não
escrito e endereços não resolvidos também têm máscara zero. O fixup USE
simbólico corresponde aos três registros originais com máscaras 0xf/f0/7ff00;
os helpers existentes de endereço lower19 não foram substituídos. Nenhuma
imagem completa executável ou API de upload foi criada. O módulo fica isolado,
sem alterar o build padrão, Gallium, ADMISSION ou gates.

Políticas explícitas: até 16 entradas, componentes R 1–4, ids de byte inteiro,
índices de codec 0–63; não são limites de hardware comprovados. Formatos de
id10/11 devem compartilhar o mesmo valor de state+124. task_word1 deve reter
o OR temp_count<<27. Erros e alias de buffers preservam o output.

**20 casos diferenciais contra o x86 original passaram em nativo, UBSan e
i386/QEMU.** Foram comparados DATA definido, extensão, todos os A2/64, índices
observados, posições e argumentos de relocation, background USE e holes.
Os testes negativos cobrem limites, formatos incompatíveis, campos de estágio
indevidos, sobreposição de buffers e preservação transacional. Não houve GPU.
`REPRODUCE.py` reproduziu a rodada completa; seus resultados autoritativos
estão em `reproduced/validation/RESULTS.json`. Hashes e patch incremental no
manifest do pacote offline; fontes anteriores e evidências preservadas.

## Limite residual e próximo discriminante

Foi refutada a interpretação de “+64” como aritmética e de R/T diretamente
alterarem bits de A2. A construção CPU também contradiz um fim global simples
no bit11 de DATA, pois pode emitir outro A2 depois dele. Nada disso prova uma
semântica física alternativa. Componentes fora do subconjunto e ids completos
podem sobrepor flags; marcadores não são teoremas iff-last para dados arbitrários.

O perfil sem R/T contém somente os dois DWORDs opacos inicial/final: A2 não
é necessário nesse perfil mínimo. Para CODE executável faltam o read set,
despacho/condições de tarefa de 0x07000345 e a semântica de término. Este replay
CPU não pode provar esses fatos; ENOTSUP de execução permanece correto.

**Próximo experimento offline:** rastrear o consumidor R no prelúdio USE
`0x36f32`, com os estados naturais 3/5, seguindo resultado+bc e link+6f até
operandos cuja semântica USSE já seja admitida. Distinguir “R fornece atributos
consumidos pelo USE” de “R é apenas notificação de testes”. Não copiar suas
sequências nem interpretar opcodes desconhecidos. Isso procura um consumidor
independente do construtor A2; repetir zero/não-zero não resolve a incógnita.
Portos, sincronização e término físicos continuam UNKNOWN até evidência própria.
