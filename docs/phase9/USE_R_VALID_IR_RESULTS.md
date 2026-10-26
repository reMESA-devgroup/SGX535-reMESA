# Feature R com IR próprio válido

**CONFIRMED (CPU): o mesmo shader constante válido atravessa o frontend e o compilador originais com feature off/on. No caso on, a reserva efetiva é 0,1,2, P=4 e o prelúdio usa P−4=0/P−2=2.** Esta rodada elimina a limitação do experimento anterior com lista intermediária vazia injetada. Entrega física R→USE e semântica de execução PDS permanecem UNKNOWN; GPU=ENOTSUP.

## Entrada independente e validade

O teste representa `MOV result.color, constant0; END`, com constant0={0,1,0,1}, como **IR Mesa próprio**, não como bytes de um programa GPU. `OWN_MESA_IR.bin` contém somente duas instruções Mesa de alto nível, 64 bytes cada. O objeto é inicializado pelos helpers originais `_mesa_init_fragment_program` (ELF107c49), `_mesa_init_instructions` (10c3b7), `_mesa_new_parameter_list` (10e1d4), `_mesa_add_unnamed_constant` (10dec8). A contagem de operandos é consultada nos helpers Mesa originais: MOV=1 fonte/1 destino; END=0/0.

Há uma referência independente: `PUBLIC_MESA_IR_ABI.c` inclui o cabeçalho **público e MIT** Mesa 7.4, preservado com URL/hash/licença em `public-mesa/`. Compilado para i386, seu `.rodata` de 128 bytes coincide integralmente com o IR inicializado pelos helpers do ELF. Static assertions verificam stride64, SrcReg+12, DstReg+36, MOV43 e END25. O SHA-256 do IR é `20b841f7bed88f7787f625fbe21446505e017426996af1171f881d28a2a1dbcf`. O layout é, portanto, confrontado com um cabeçalho público independente e com código do binário; não é uma conclusão de um validador próprio isolado.

O frontend **inteiro** ELF33490 traduz esse IR e chama3503d com duas instruções internas (classes CPU72/53). O compilador **inteiro** 1c6b56 retorna0, observado em35283. A lista de descritores chega ao alocador237b63 com14 entradas; o otimizador original a reduz a zero para este shader constante. Nenhuma lista vazia é injetada. As cópias P/C são produzidas pelo compilador; C não é fornecido pelo harness.

## Diferencial observado

Só a feature compile-key bit1 é variada; shader, constantes e todos os demais parâmetros de entrada permanecem idênticos. A cópia de key para compiled-object+14 é executada pelo bloco original2cb36–2cb39. A transformação compile-key bit1→link-key bit2 é executada pelo bloco original2c683–2c691. Esses são estados de chave do chamador; não são alterações no IR intermediário do compilador.

| Campo observado | Feature off | Feature on |
|---|---:|---:|
| Compile key / link key | 0 / 0 | 2 / 4 |
| IR interno antes de3503d | 2 instruções | mesmas2 instruções |
| Flags / classe | 0x4a / 1 | 0xca / 2 |
| Quantidade reservada params+90 | 0 | 3 |
| Stores reais do mapa em238d8b | nenhum | 56→0, 57→1, 58→2 |
| P / C produzidos | 0 / 0 | 4 / 0 |
| Posição adicional ctx+5f8 | sem reserva | 3 |
| Link+6f / +70 / +71 | 1 / 0 / 1 | 1 / 3 / 4 |
| Link+74 (usado no DATA) | 1 | 0 |
| Operand observations36f32 | não chamado | P−4=0, P−2=2, S+1=2 |
| DATA PDS | 12 DWORDs | 12 DWORDs |
| CODE PDS original construído | 07000345, af000000 | 07000345, 070418a2, af000000 |
| A2 / índice | nenhum | 1 / j=1 |
| R adicional em DATA[9] | não definido | 0x0f80e00f |
| Relocation USE background | 0 | 0x80000 |

A cadeia segue do frontend ao compilador, depois ao linker original370d8 e ao construtor PDS original25970, consumindo os metadados resultantes. As APIs são invocadas separadamente nos seus limites de chamador; **não há reseed de estado intermediário de compilação**. Alocação, commit e BO bookkeeping do construtor PDS são serviços CPU sintéticos explicitados no JSON. Os registros de relocation são escritos pelo código original e continuam simbólicos.

O teste é repetido com malloc preenchido por0 e0xa5 (calloc permanece zero). Ambos produzem metadados/PDS idênticos para cada feature. Foram executadas entre331 mil e435 mil instruções x86 por caso, sempre em Unicorn; não há acesso ao Mini12 nem execução do binário pelo loader do host.

## Comparação com encoder original do projeto

`VERIFY_VALID_IR.py` compara os campos conhecidos de `remesa_pds_structure_encode` contra esses quatro resultados: off8 bytes DATA; on12 bytes DATA e4 bytes CODE A2. Todos coincidem byte a byte e os backgrounds simbólicos coincidem. Initial/final CODE permanecem duas lacunas explicitamente excluídas; a comparação **não** declara emissão executável.

A validação central do parent ampliou essa comparação ao novo serializer/encode_code em nativo, UBSan e i386, em `own-ir-code-comparison-frozen/RESULTS.json`; sua fonte-oráculo é o mesmo SHA-256 congelado do `VALID_IR_RESULTS.json` (`7d5efec92c33a487bffe6f946b204e874f0ab1288786d735b435db751c443d8f`).

**REFUTED neste caso:** a correspondência P−4/P−2 dependeria exclusivamente do antigo estado sintético vazio; a feature R alteraria necessariamente o DWORD inicial07000345. Este próprio shader válido conserva07000345 off/on, enquanto altera A2, DATA e relocation.

**INFERRED:** R(id14,n3) solicita os valores do grupo de três posições reservado pela feature. A correspondência CPU está agora validada num shader real de IR; ainda não identifica o mecanismo físico de entrega.

**UNKNOWN:** banco físico e nomes dos três valores, ordem/momento de entrega, significado GPU dos marcadores e das classes45/A2/AF. Não foram exportados ou reutilizados os words USE gerados pelo compilador. Eles ficam somente na RAM privada do emulador e não integram o driver ou os artefatos exportados.

## Reprodução e tentativas preservadas

Execute `REPLAY_VALID_IR.py` e `VERIFY_VALID_IR.py`. O primeiro é o replay CPU original; o segundo verifica ABI pública e os resultados retidos. `COMMANDS.json` registra compilação i386 da ABI, compilação nativa do probe, verificações e logs. `INPUT-CHECK.json` verifica o binário e todos os artefatos do subdiretório anterior use-delivery.

Tentativas intermediárias são preservadas, mas não servem como prova final: initial bloqueou em PLT_setjmp por allowlist;02 completou os dois compiladores;03 teve erro de aridade do harness no linker (faltou o primeiro argumento não usado), portanto seus campos linker são inválidos;04 corrigiu a assinatura e acrescentou guardas contra acessos baixos;05 estendeu ao construtor PDS. `REPLAY_VALID_IR.py` é a versão final, com assert de todos os quatro casos e observações dos stores reais. Nenhum desses erros tocou hardware ou artefato histórico.

## Handoff de endereços

Binário: psb_dri.so, SHA-256 `74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8`. Ghidra image base/bias +0x10000; file offsets exatos em `ADDRESS-LOCATIONS.json`.

| Função / consumidor | ELF | Ghidra | Última evidência CPU |
|---|---:|---:|---|
| Frontend FS | 33490 | 43490 | IR público próprio traduzido e compilado |
| Wrapper FS | 3503d | 4503d | mesmas2 instruções, flags4a/ca |
| Reserva3 | 351ca/351d4 | 451ca/451d4 | params+8c/+90=3 |
| Alocador | 237b63 | 247b63 | execução completa,14 descritores na entrada |
| Store do mapa | 238d8b | 248d8b | `mov [ebp+eax*4-354],edx`:56→0,57→1,58→2 |
| Padding | 2390cd | 2490cd | `mov [eax+5f8],edx`:3 |
| Cópia P | 35383 | 45383 | `mov [ecx+bc],eax`:P4 |
| Linker | 370d8 | 470d8 | owner+70=3; mantém metadados reais |
| Prelúdio | 36f32 | 46f32 | operandos P−4/P−2/S+1=0/2/2 |
| Construtor PDS | 25970 | 35970 | DATA e classes45/A2/AF observados |

**ABRIR NO GHIDRA:** psb_dri.so, G=`0x46f32`, função ELF36f32. A aritmética dos índices já foi reproduzida com IR válido; a pergunta restante é qual banco/fonte física as posições0/2 representam. Isso não pode ser concluído somente dos índices CPU. Para ampliar o encoder PDS, o parent deve priorizar a especificação das classes45/AF, não o primeiro PC físico.
