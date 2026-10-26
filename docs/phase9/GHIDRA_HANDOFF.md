# Ghidra handoff: emissão de CODE e limite de45

## ABRIR NO GHIDRA

Abra `/home/gama/sgx535-offline/pds-encoder-implementation-20261019/isa-audit/ghidra-project/XpsbIsaResearch.gpr`, programa **Xpsb.so**, e use **G → 0x189cf**, função `Xpsb_emit_pixel_shader` (`0x182c0`). Pergunta: **as fontes `a,b` da classe64 podem variar independentemente nos caminhos válidos e como essa variação ocupa bits que atravessam o byte baixo?** A transformação é demonstrável no bloco e pode ampliar o encoder de campos sem inventar o significado ISA de45.

Para verificar por que45 ainda não é emissível semanticamente, use **G → 0x18953**. A instrução é `MOV dword ptr [ESI],0x07000345`, um store literal. Não há parâmetro de fonte ou operação para recuperar nessa instrução.

## Binários, mapeamento e identidade

| Nome / caminho original | SHA-256 | Base Ghidra |
|---|---|---|
| `/home/gama/sgx535-offline/xpsb-structure-20261009T225201Z/Xpsb.so` |da531587b1ec59fe433fa20ddd2e691b2f8b7fb35bb82525d4db99259c5e571f|0x10000|
| `/home/gama/sgx535-gfx/references/home:lkundrak:poulsbo/xpsb-glx/extracted/xpsb-glx/dri/psb_dri.so` |74ca42991906741ee91be1bc088ae0b142fa71b6a85658c5dad0851ce50dd0d8|0x10000 no dossier retido|

ELF VA e file offset coincidem nestes trechos do PT_LOAD executável, demonstrado em `LITERAL-LOAD-SEGMENT-SCAN.json`. Endereço virtual do projeto = ELF VA+0x10000; nenhum destes valores é um endereço GPU. A cópia privada Xpsb foi consultada com Ghidra12.1.4 `-readOnly -noanalysis`; a consulta terminou com exit0. O programa psb não foi reimportado: seu dossier REA completo foi reutilizado (`ev_a99aba2b79a5978af8c5e0c98b2d9aeaac21b44ee38c8f54edb3761d2a10bfac`).

## Âncora45 e p-code

- Função: **Xpsb_emit_pixel_shader**.
- Entry point: ELF/file82c0; virtual Ghidra182c0.
- Basic block real: Ghidra188f9–18972 inclusivo, ELF88f9–8972.
- Store: ELF/file8953; virtual Ghidra18953.
- Instrução: `c7 06 45 03 00 07` / `MOV dword ptr [ESI],0x07000345`.
- P-code original de instrução: `tmp=COPY const0x07000345; STORE ram,ESI,tmp`.
- Registradores: ESI é destino CPU; EAX aponta o cursor CPU; antes do store, `add esi,[eax]` em ELF8951 soma a base a q. EBP ancora os locais; não há DS físico identificado nesse bloco.

Pseudocódigo descritivo (nomes nossos, não source proprietário):

```c
n = tamanho_DATA_alinhado;             // DWORDs
q = n * 4;
p_code = cursor_CPU + q;
store_le32(p_code, literal_observado); // a instrução original usa imm32
for (i = 0; i < T_count; ++i) {
    store_le32(p_code + 4 + 8*i, campo_A2(j[i]));
    store_le32(p_code + 8 + 8*i, campo_classe64(a[i], b[i]));
}
store_le32(fim, literal_AF_observado);
```

Os dois nomes `literal` documentam a observação; não são proposta de implementação.

## Xrefs comprovados da função Xpsb

| Caller / entry Ghidra | Call site Ghidra | ELF/file |
|---|---:|---:|
| psb3DPrepareComposite /15ee0 |16259|6259|
| psb3DPrepareComposite /15ee0 |16331|6331|
| psbBlitYUV /164f0 |16803|6803|
| psbBlitYUVDetear /16b10 |16e71|6e71|
| psbBlitYUVDetear /16b10 |16f57|6f57|

Lista retornada diretamente por Ghidra nesta rodada, em `GHIDRA-ENCODING-AUDIT.tsv`; referência externa Entry Point também foi preservada.

## Próximo bloco útil de campos variáveis

- Mesmo programa/função/entry.
- Basic block Ghidra18998–18a20 inclusivo; ELF8998–8a20.
- Primeira fonte A2: ELF/file89a4, Ghidra189a4: `MOV EAX,dword ptr [EDX + EDI*4]`.
- Segunda fonte classe64: ELF/file89cf, Ghidra189cf: `MOV ESI,dword ptr [EAX + EDI*4]`.
- Outra fonte classe64: ELF/file89d2, Ghidra189d2: `MOV ECX,dword ptr [EDX + EDI*4]`.
- Registers: EDI=i, ESI=b, ECX=a; EDX/EAX são ponteiros CPU para tabelas de índices antes dos shifts, depois acumuladores.
- A2 store: ELF/file89c5, Ghidra189c5.
- Classe64 store: ELF/file8a11, Ghidra18a11.

Expressão de campos demonstrada, sem nome ISA:

```text
C64(a,b) = 0x07000064
         | ((a>>1)<<18) | ((b>>1)<<13)
         | ((a&1)<<11) | (((a+1)&1)<<9) | (((b&1)+2)<<7)
```

O psb usa o mesmo esquema produtor no bloco do dossier Ghidra3637d–363cf (end exclusivo da lista REA), store Ghidra363c4 = ELF/file263c4. Caller de `FUN_00035970` = `FUN_000383d8`, callsite384ee = ELF/file284ee. A2 psb está no bloco36345–36375 (end exclusivo), store3636a = ELF/file2636a. Evite comparar lowbyte64 como identificador invariável: b ímpar leva `e4` em Xpsb.

## Última evidência confirmada e primeiro limite

**CONFIRMED:** memória CPU, tabelas e shifts/ORs que constroem A2/64; quatro produtores literais de45; variações de metadata/slots/features não mudam45 nas11 execuções delimitadas.

**Primeira transformação desconhecida para ampliar45:** a interpretação ISA do literal, incluindo campos de opcode, seleção de fontes, porta e condição. Não existe uma transformação x86 oculta a partir de operandos no store8953; o valor já está completo no immediate. Não se atribui um endereço de instrução CPU inexistente a esse decodificador hardware. As hipóteses MOVS/ALWAYS/DOUTU continuam UNKNOWN. O término AF também não tem regra semântica demonstrada.

Próximo experimento CPU útil: reproduzir os três índices reais de classe64 em dois produtores com fontes que mudem paridade independentemente, e confrontar a emissão calculada sem um decoder baseado na mesma hipótese. Isso amplia a cobertura de campos verificáveis, mas não fecha45. Para fechar45 é indispensável outra fonte independente que varie ou especifique seus operandos. O primeiro PC físico permanece UNKNOWN e fora desta auditoria.

GHIDRA NEXT: Xpsb.so | Xpsb_emit_pixel_shader | 0x189cf | MOV ESI,dword ptr [EAX + EDI*4] | Quais caminhos válidos permitem variar a segunda fonte da classe64 independentemente e confirmar os bits sobrepostos ao byte baixo?
