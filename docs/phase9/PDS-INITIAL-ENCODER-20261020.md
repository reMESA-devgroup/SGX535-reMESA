# PDS encoder — prefixo parametrizado, AF ainda bloqueado

**PDS_ENCODER_PARTIAL_EVIDENCE.** Foi implementada emissão original de uma nova palavra CODE calculada, incluindo07000345, e refinamento opt-in de estruturas existentes. **Nenhum programa PDS completo foi emitido.** `remesa_pds_encode()` e submissão GPU permanecemENOTSUP. Não se usa o primeiro PC como bloqueio.

## Descoberta que muda a implementação

A âncora Xpsb Ghidra18953 é ELF/file8953, `MOV [ESI],07000345`. Seu valor não recebe operandos: metadata e slots afetam DATA, posição do store e palavras posteriores. Ghidra12.1.4 confirmou por P-code/SSA o store de constante; `linux/GHIDRA-XPSB.tsv` inclui callers, pseudocódigo e blocos. Não se criou um encoder genérico decompondo esse imediato.

A ligação nova vem de **outro produtor parametrizado**: `libEMGDOGL.so.1.5.15.3226`, SHAaea32db282ee55a4c3075906295c438e8e94a2bd8ed3eac42be46c0582b88e1d, ELF13f470, Ghidra14f470. O bloco13f5b3..13f5f4 calcula:

```text
W = 07000205 | (a <<17) | ((b >>1) <<13)
             | (b ímpar ? 60 : 40)
             | (b ímpar ? 180 : 100)
```

O x86 efetivamente escreve o par de valores de record+34/+38 nas células `cell0(a),cell0(a+1)` em13f63c/13f656, e record+3c em`cell1(b)` em13f66b. Os índices usados pelo cálculo de CODE provêm dos mesmos contadores. Isso demonstra **referências do construtor CPU**, não leituras físicas DS. `cell0(k)=16*(k/8)+k%8`, `cell1(k)=cell0(k)+8`, em DWORDs.

| Entradas do construtor | a,b capturados antes do store | Palavra original |
|---|---|---|
| count0,optional0 |0,0|07000345|
| count0,optional1 |0,1|070003e5|
| count1,optional0/1 |2,0/1|07040345/070403e5|
| count2,optional0/1 |4,0/1|07080345/070803e5|
| count3,optional0/1 |4,2/3|07082345/070823e5|

Os oito resultados foram obtidos executando as instruções x86 originais do construtor e do helper13f330 em Unicorn, sem serviços externos. Hooks apenas observam operandos e stores. São registros delimitados da **interface do construtor**, não oito shaders Mesa de produção nem prova de alcançabilidade de cada combinação num contexto GL. Não se injetaram estados intermediários. Os callers reais12c09c/12c727/12c765 foram identificados; a enumeração integral de seus estados de contexto continua incompleta.

A referência independente Windows `igdumd32.dll` SHA83873d2196c5d3ed2a1735d2c60d69e6b8242942ee4c16c4b84c9d70f2408fc9, PE VA100a603f..100a6068, corrobora os seis resultados com b0/1: `07000345|(a<<17)|(f<<5)|(f<<7)`, a0/2/4,f0/1. Há possível linhagem comum; não é confirmação independente da ISA/hardware.

## Campos comprovados do produtor CPU

No domínio restrito implementado:

| Bits | Expressão/valor CPU |
|---|---|
|0..4|05|
|5|b&1|
|6|1|
|7|b&1|
|8..9|3|
|10..12|0|
|13|b>>1, limitado0/1|
|14..17|0|
|18..19|a>>1, limitado0/1/2|
|20..23|0|
|24..26|7|
|27..31|0|

Não se nomeiam bits24..26 como condição ALWAYS nem os bits baixos como opcode MOVS/DOUTU. Bits5/7 estão acoplados **neste produtor**; outros produtores conhecidos os variam separadamente. O helper13f370/17790 é outra família e não é usado como justificação para essa codificação. O preview hipotético antigo também não é oráculo.

## Código implementado e integração

Em `remesa_pds_structure.c/.h`:

- `remesa_pds_structure_pair_single()` implementa a expressão produtora;07000345 não aparece como template na geração.
- Novo kind tipadoPAIR_SINGLE no emissor CODE existente. Permite apenas a0/2,b0/1 ou a4,b0..3; a ímpar, combinações desconhecidas e limites são rejeitados. Esse domínio é política CPU conservadora, não limite de ISA.
- `encode_pair_single_output()` deriva os índices das células DATA tipadas e emite little-endian pelo mesmo emissor.
- `resolve_initial()` refina opt-in o primeiro slot do layout psb existente usando DATA0/1/8. Mantém o corpo A2/classe64, relocations simbólicas e todos os demais dados/máscaras. O último slot AF continua zero **com máscara zero**, não é NOP nem instrução final emitida.
- Validador/serializador reconhecem a nova primeira palavra calculada. Falhas preservam saídas e rejeitam alias/ranges/estruturas corrompidas.

O teste novo recebe a saída do alocador recuperado14R+T, refina e serializa o prefixo, sem reimplementar alocação ou relocations. O codec foi incorporado ao arquivo Mesa25.2.0 real, com testes vinculados a ele. O caminho Gallium padrão mantém o comportamento anterior; não ativa automaticamente o refinamento, não acrescenta amostragem NIR e não cria um contexto OpenGL funcional.

A validação normal do pipeline e os gates permanecem intactos. O status de erro físico de despacho/término não é convertido em sucesso por esta API estrutural. Liveness automática não foi necessária para essa emissão; sua investigação foi adiada para manter a prioridade no CODE.

## AF: limite exato

Xpsb ELF8a47, psb ELF263e4, OGL ELF13f5f9 e os construtores Windows auditados armazenamAF000000 como **imediato**, sem entrada variável nos seus bits. A existência/posição de words anteriores e o cursor variam; isso não fornece campos de AF.

O Windows100a648d..100a64c7 também apresenta paddingB0000000 opcional até o corpo atingir7c bytes, depoisAF. Outro perfil100a7190 armazena doisAF separados por CODE adicional. Logo, nem a posição de AF nem sua vizinhança autorizam interpretá-lo como metade fixa de uma instrução64-bit ou como terminador global.

**UNKNOWN/BLOCKED:** partição de opcode/condição de AF; largura/enumeradores da condição; significado dos24 bits baixos zero; operandos, leituras implícitas, despacho/sincronização e efeito físico de término. Não existe nesta evidência produtor parametrizado ou decoder aplicável que resolva isso. Emissão de AF por literal preencheria precisamente a lacuna que o pedido proíbe. Por isso não foi declarada emissão completa nemEXECUTABLE_CANDIDATE_OFFLINE.

O scan ASCII delimitado em Xpsb,psb,OGL e os componentes Windows examinados não encontrou decoder ligado a HALT/MOVS/DOUTU; resultados e limites estão em `linux/BOUNDED-MNEMONIC-SCAN.json` e `windows/REPORT.md`. Não é prova de ausência global. Nenhum DDK ou conteúdo de firmware/shader foi utilizado como fonte.

## Testes executados

- 70 checks novos por ambiente: nativo,UBSan,i386/QEMU e arquivo Mesa real.
- 128 comparações independentes:8 palavras OGL +20 imagens históricas +4 shaders constantes originais, em4 ambientes. AF permanece excluído explicitamente. O refinamento acrescenta80 bytes CODE conhecidos aos20 oráculos por variante; os304 bytes anteriores continuam comparados.
- Quatro shaders próprios MOV constante verde/magenta, feature off/on, passaram pelo compilador/linker/construtor psb originais. ABI pública Mesa7.4 compilada separadamente parai386:4/4,512 bytes de IR coincidentes. CODE muda[345,AF]→[345,A2,AF] com a feature; não há prova isolada de operação física dessas palavras.
- Regressões mantidas:115 classe64,113 alocador,1458 pipeline por variante;377452 USSE/PDS por variante;20 oráculos/588 bytes DATA/304 CODE legados em nativo,UBSan,i386.
- Mesa25.2.0 estático e DRI compilados. Testes do arquivo real:70 novos,115 classe64,113 alocador,1458 pipeline,340 pixel,207 host,32 NIR. Segunda execução Ninja real:891 artefatos, nenhum hash alterado. Não houve instalação.

Logs/argv/hashes estão em `TEST_RESULTS.json`, receipts e manifesto. O aviso LOAD RWX do DRI já estava no baseline; não foram alteradas flags do linker. Os659 artefatos do checkpointR-allocator são preservados e verificados antes/depois.

## Classificação e continuidade

**CONFIRMED:** expressão CPU parametrizada, fontes DATA do writer, domínio dos branches, posição/valor dos stores, comparações originais e Windows delimitadas. **IMPLEMENTED_OFFLINE:** codec, IR, refinamento inicial, serialização e integração ao arquivo Mesa. **INFERRED:** mesma função de hardware entre produtores/linhagem compartilhada. **UNKNOWN:** mnemonic, porta, aridade física, DATA→DS,AF e first_pc. **BLOCKED:** programa completo pelo requisito de não copiarAF sem regra de codificação justificada.

Próxima evidência indispensável: um encoder parametrizado ou decoder SGX535 aplicável que relacione explicitamente condição/operandos a AF/variantes. A última instrução observável é Ghidra14f5f9, `MOV [EAX],AF000000`; repetir esse replay não identifica seus campos. `GHIDRA_HANDOFF.md` distingue essa fronteira do produtor parametrizado já resolvido em14f5b3..14f5f4.

Zero SSH,MMIO,kicks,submissões GPU,módulos,alterações de kernel/sudo. ADMISSION e gates permanecem inalterados; GPU=ENOTSUP.
