# 0x07000345: regra CPU confirmada; operandos ISA ainda não demonstrados

## Resultado

**CONFIRMED:** no Xpsb analisado e nos três produtores encontrados no `psb_dri.so`, o DWORD é produzido por `MOV r/m32, imm32`. Não há shift, máscara, OR, seletor de slot ou chamada a assembler que construa seus campos nesses stores. A investigação não autoriza um encoder semântico de `0x07000345`. O caso deve permanecer `ENOTSUP`; as classes A2/64 com expressões produtoras variáveis podem ser emitidas separadamente como codificação estrutural CPU.

Esta auditoria não altera o worktree, não emite programa GPU e não reabre o primeiro PC. Não considera as fixtures do construtor como IR válido de produção.

## Identidade e endereços

| Binário | SHA-256 | Base Ghidra |
|---|---|---|
| Xpsb.so | da531587b1ec59fe433fa20ddd2e691b2f8b7fb35bb82525d4db99259c5e571f | 0x10000 |
| psb_dri.so | 74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8 | 0x10000 |

O produtor histórico referido como **0x18953** é endereço Ghidra, **ELF VA/file offset 0x8953**, no Xpsb. Não é ELF 0x18953. O PT_LOAD executável começa em file offset/ELF VA zero nos dois arquivos; por isso os offsets de arquivo citados aqui coincidem com os ELF VAs. Não são endereços da SGX.

## Produtores diretos e def-use

| Função / entry ELF | Store ELF = file offset | Ghidra | Operação produtora | DWORD seguinte |
|---|---:|---:|---|---|
| Xpsb_emit_pixel_shader / 82c0 | 8953 | 18953 | `mov dword ptr [esi],0x07000345` | AF sem T; A2(j) se houver T |
| psb_emit_ps_primary_compiled (nome por assertion) / 25970 | 262f0 | 362f0 | `mov dword ptr [eax],0x07000345` | A2(j) se máscara ativa; AF se não houver |
| produtor fixo sem T / 39ac4 | 39bd2 | 49bd2 | `mov dword ptr [esi+0x30],0x07000345` | AF em +34 |
| produtor fixo com T / 39c5c | 39e0c | 49e0c | `mov dword ptr [esi+0x30],0x07000345` | A2 em +34; classe64 em +38; AF em +3c |

O scan `LITERAL-LOAD-SEGMENT-SCAN.json` encontrou **uma ocorrência literal** na região executável Xpsb e **três** na região executável psb. Este é um limite explícito da busca: não prova ausência de um assembler que gere o mesmo resultado sem carregar esse literal ou de outro código em binário não auditado.

No Xpsb, `ESI=q` antes de ELF8951; `EAX` aponta para o cursor CPU; `add esi,[eax]` forma o destino CPU. **ESI é o endereço do store, não um operando PDS reconstruído.** O p-code Ghidra em18953 é `COPY const0x07000345; STORE ram,ESI,value`, sem inputs de metadata para o valor. Depois se salva o ponteiro em `[ebp-0xc4]`, avança quatro bytes e testa a contagem de T. No psb, ELF262ed faz `lea eax,[esi+4*eax]`, onde ESI é base DATA e EAX o tamanho alinhado em DWORDs; ELF262f0 escreve o literal. Mudar n só muda o endereço CPU do store.

## Comparação executada nesta rodada

`REPLAY_45_INVARIANCE.py` chama os harnesses históricos verificados e executa instruções **x86 originais** em Unicorn, com allowlist e memória sintética. Sem biblioteca proprietária carregada no host e sem API de GPU. Alocação/BO e serviços externos continuam interceptados conforme os harnesses preservados; a chamada não é uma execução PDS. O log real está em `replay-45.log` e os resultados em `REPLAY-45-INVARIANCE.json`.

| Produtor / diferença controlada | DATA n | Resultado no primeiro store CODE | Diferença útil |
|---|---:|---|---|
| Xpsb T=0 → 1 |12 →12|07000345 em ambos|DWORD seguinte AF → A2(2)|
| Xpsb T=1 → 2|12 →16|07000345 em ambos|cresce o prefixo DATA e mais A2/64|
| Xpsb T=2 → 3|16 →16|07000345 em ambos|mais A2/64, mesmo n|
| Xpsb metadata6=0 →1, sem T|12 →12|07000345 em ambos|DATA[1] muda para08000000|
| Xpsb constantes=0/1/2, sem T|12 em todos|07000345 em todos|varia o segundo programa, não o CODE primário|
| psb T fixo, extra R=0 →3|12 →12|07000345 em ambos|novo A2(3) e descritor R, não muda45|
| psb T → R+T fundidos|12 →12|07000345 em ambos|DATA e background variam; CODE do T permanece|
| psb zero vs T|12 em ambos|07000345 em ambos|AF imediato vs A2(1)/64/AF|

São **11 execuções delimitadas**, não reexecução das suítes de regressão. As entradas são metadata sintética do construtor, não a feature R aplicada a IR próprio válido. Essa tarefa é independente e não é satisfeita por estas fixtures.

O par de produtores fixos é uma segunda comparação estática independente: um compila e liga um USE sem T, o outro acrescenta estado/relocation de textura e dois DWORDs CODE, mas ambos carregam o mesmo imediato45. Isso contradiz a dependência direta de45 no número de T nesses caminhos. Os arquivos `.asm` novos são extrações do binário, usados apenas como evidência.

## Campos e hipóteses

| Bits de45 | Expressão demonstrada | Classificação CPU | Interpretação de ISA |
|---|---|---|---|
|31:0|imediato fixo `0x07000345`|CONFIRMED|UNKNOWN|
|qualquer particionamento interno proposto|nenhum produtor por campos encontrado nestes quatro stores|UNKNOWN|não admitido para emissão|

Não existe nesta evidência uma fronteira de opcode, condição ALWAYS, seletor DOUTU, banco DS ou aridade. Desmembrar o imediato e recombiná-lo por nomes inventados daria os mesmos bytes sem acrescentar prova.

- **REFUTED, escopo auditado:**45 contém diretamente o número de texturas, n, metadata6 ou a flag extra R, de forma que esses deltas precisem mudar seus bits.
- **INFERRED:**45 tem uma função fixa neste perfil e seus dados variáveis são preparados separadamente. Isso é compatível com opcode de tarefa com fontes fixas, mas também com outra operação de fontes implícitas ou um elemento de protocolo maior.
- **UNKNOWN:**se45 lê DATA/DS0,1,8; se tem três fontes; se dispara USSE; se é MOVS; se a condição é ALWAYS; se o destino é DOUTU.
- **UNKNOWN:**significado ISA de AF, inclusive sua condição de término. Todos os produtores auditados também o carregam como imediato.

Uma hipótese falsificável é:45 é uma saída fixa de tarefa que consome as três palavras de controle preparadas no prefixo DATA. Uma referência independente que identifique bancos, índices e porta de saída poderia confirmá-la; um produtor legítimo que altere uma dessas fontes e revele uma codificação incompatível a refutaria. A invariância CPU observada é insuficiente para separar essa hipótese de fontes implícitas ou de uma etapa de protocolo maior.

## Consequência para implementação

Emissão A2/64 deve derivar dos índices observados, com validação e fontes tipadas e sem promover os nomes de classes a instruções físicas. Por exemplo, o Xpsb produz A2(2)=070430a2 enquanto o psb usa A2(1)=070418a2 para o primeiro T; o DWORD da classe64 do Xpsb é070403e4 porque um campo variável sobrepõe o byte baixo64. Classificar toda a família por `(word & 0xff)==0x64` é incorreto. Essa diferença pode validar o codec de campos separadamente.

O emissor de programa completo deve recusar45/AF enquanto não houver regra de operandos independente; preencher os dois holes com bytes históricos continuaria sendo transplante, mesmo que fosse chamado encoder restrito. Nenhum novo bloqueio de PC físico é usado para justificar esta decisão.

## Ponto de continuação

Ghidra Xpsb `0x189cf` carrega a segunda fonte variável da classe64; o bloco `0x18998–0x18a20` permite testar variantes com fontes pares/ímpares e bancos, contrastando psb `0x3637d–0x363c4`. Para45, volte a `0x18953` apenas como âncora da evidência negativa: não há operandos codificados dinamicamente ali. A próxima prova necessária para45 é uma referência independente de sua operação/fontes, não outro replay do mesmo `mov imm32`.
