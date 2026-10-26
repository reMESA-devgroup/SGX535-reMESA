# PDS CLASS64 semantic recovery — 2026-10-19

**CLASS64_PARTIAL_EVIDENCE.** Há uma nova ligação CPU DATA/relocation → campos
de CODE implementada e validada. A independência dos dois operandos dentro de
um produtor de produção ainda não foi demonstrada; a operação física da ISA
continua UNKNOWN. Não declaramos recuperação completa da semântica classe64.

## CONFIRMED: produtores e referências

O projeto Ghidra12.1.4 foi copiado e consultado readOnly, sem reimportação ou
mudança do projeto anterior. Assembly, P-code, SSA e xrefs estão em
`producers/GHIDRA-CLASS64.tsv`; processo exit0. O manifesto anterior de799
artefatos e seus arquivos-fonte foram conferidos antes desta rodada.

Em Xpsb.so, função ELF82c0/Ghidra182c0, a carga ELF89cf/Ghidra189cf lê uma
tabela de índices, não uma palavra do programa. EAX vem da tabela automática
`[ebp-6c]`, EDI é o ordinal da textura. Três stores incondicionais dão:

| Fonte | Store ELF | Regra alcançável | Papel na construção DATA |
|---|---:|---|---|
| a | 85bf | 2+2*i | par de estado em cell0(a),cell0(a+1) |
| b | 85d1 | 1+2*i | destino da relocation do endereço de textura em cell1(b) |
| j do A2 | 85d8 | b+1=a | descritor complementar em cell1(j) |

`cell0(k)=16*(k/8)+(k%8)` e `cell1(k)=cell0(k)+8`, em DWORDs CPU.
Os formatos aceitos convergem em83f8 antes desses produtores. O último T
altera o marcador DATA em8668, não os índices. Assim, variar somente formato,
dimensões ou quantidade de texturas **não** fornece independência de a/b nesse
produtor. Nos caminhos Xpsb auditados ambos avançam juntos.

No produtor independente psb_dri.so ELF25970, os loads2637d/26384 são registrados
antes dos shifts em26388, e o store final é263c4. Aqui o descritor vem antes do
endereço: **j=b−1**. As chamadas originais37854 confirmam a célula da relocation.
Os writers de estado são25f4b/25f7f, ou25c71/25c9f no caminho R/T fundido.

Nova relação reproduzida: **b acompanha a posição DATA destinada ao endereço
relocável, não o identificador do sampler**. O sampler0→1 altera o primeiro
byte do descritor compilado e o BO da relocation60074400→60074500, mas mantém
destino DATA+40, a=2,b=2 e word07042364. Trata-se da semântica do contrato CPU;
não prova que uma instrução GPU efetivamente leia essa célula.

## CONFIRMED: byte baixo e limites das interpretações

A expressão por índices já era conhecida; esta rodada amarra os campos às
referências DATA e aos caminhos que realmente os produzem:

```text
W=07000064 | ((a>>1)<<18) | ((b>>1)<<13)
           | ((a&1)<<11) | (((a+1)&1)<<9) | (((b&1)+2)<<7)
W&ff = 64 | ((b&1)<<7)
     = 64 | ((texture_DATA_cell&1)<<7)
```

| Bits | Expressão CPU no domínio admitido0..63 |
|---|---|
| 0..6 | constante64 |
| 7 | b&1, igual à paridade da célula DATA da relocation |
| 8 | 1 |
| 9 | (a+1)&1 |
| 10 | 0 |
| 11 | a&1 |
| 12 | 0 |
| 13..17 | b>>1 |
| 18..22 | a>>1 |
| 23 | 0 |
| 24..26 | 7 |
| 27..31 | 0 |

Não há máscara final do byte, nem colisão entre esses campos variáveis no
domínio admitido. O campo do operando invade a região que uma leitura visual
poderia chamar equivocadamente de byte de opcode. Não se conclui o tamanho
real de opcode da ISA. Xpsb produz b ímpar/byte e4; psb válido reproduzido
produz b par/byte64. Ambos vêm da mesma expressão CPU. Significado físico de
portas, fontes DS, aridade e operação continua UNKNOWN.

## Experimentos válidos e evidência negativa

Seis programas Mesa7.4 próprios atravessaram frontend33490, wrapper3503d/compilador1c6b56,
linker370d8 e construtor25970 originais em Unicorn. Nenhuma lista intermediária
foi injetada. O cabeçalho público MIT, compilado para i386, confirmou os1024
bytes de IR dos seis casos, incluindo precisão e bitfields. Dois controles
3D/CUBE acrescentam512 bytes de IR conferidos pela mesma ABI pública; mantêm
R,T,R, a2,b2 e a mesma classe64 apesar da largura de coordenadas passar a3. Os words USE
gerados ficaram privados no emulador, sem exportação ou reutilização.

| Comparação | Resultado direto |
|---|---|
| sampler0 / sampler1, mesma coordenada | BO muda; a2,b2 e word07042364 iguais |
| coordenada0 / coordenada1 | frontend densifica o único índice; a2,b2 iguais |
| duas texturas | (a,b)=(2,2)→(4,4), words07042364→07084364 |
| TEX com coordenadas constantes | nenhum descritor de entrada PDS; nenhuma classe64 |
| ADD de duas cores antes do TEX | metadata final R,T,R, não R,R,T; a2,b2 |

Separadamente, o construtor Xpsb aceitou registros ARGB8888,RGB565,A8 e
source/mask: formatos mudam DATA, mas não a/b no mesmo ordinal. Esse replay
é do construtor com serviços BO explícitos; não é uma compilação de IR Mesa.
Seu caso adicional coordenada1 não é usado como prova de alcançabilidade do
compositor. Os quatro registros de formato utilizados são confrontados no
comparador central; resultados completos estão em `xpsb-textures/`.

**REFUTED no escopo observado:** b ser o samplerID; classificar a família
somente por byte baixo64; assumir que ordem textual ADD,ADD,TEX força R,R,T.
**UNKNOWN:** caminho válido mantendo b e variando somente a; variação isolada
de b dentro de um mesmo produtor. A diferença b1/b2 com a2 existe entre Xpsb
e psb; ela não prova independência dentro de uma mesma função.

## Implementação e validação

`remesa_pds_texture_output` descreve duas células de estado, a célula do
endereço de textura, a do descritor e uma das duas ordens demonstradas.
`remesa_pds_structure_encode_texture_output()` inverte o mapeamento DATA,
deriva a/b/j e emite A2+classe64 pelos codecs tipados existentes. Não recebe
bytecode de referência. Rejeita banco incorreto, par ímpar, task slots,
vizinhança lógica inválida, limites, alias e ordens desconhecidas.

O encoder estrutural e seu validador agora usam a célula real da relocation
para derivar b. O pipeline Gallium/NIR já existente herda essa integração.
Não foi acrescentado suporte NIR a amostragem nem execução de shaders com
textura. Codec bruto, armazenamento, ownership e rollback foram preservados.

- 115 verificações novas: nativo,UBSan,i386/QEMU e arquivo Mesa real.
- 1396 verificações de pipeline e377452 regressões por variante: PASS.
- 20 oráculos históricos:588 bytes DATA e304 CODE conhecidos coincidentes.
- 76 comparações CPU novas:52 pares por referências DATA e24 imagens esparsas,
  em nativo,UBSan,i386 e arquivo real. Lacunas e endereços simbólicos excluídos.
- Mesa25.2.0 estático/DRI e32 casos NIR reais: PASS. Logs completos em `mesa-build/`.

As tentativas com ABI ou adaptador incorretos foram preservadas e excluídas.
O comparador independente detectou offsets errados de flags no adaptador:
Rflag=m7,Tflag_a=m6,Tflag_b=m7. A correção não alterou o encoder nem os replays
originais. O primeiro comparador central também corrigiu o alvo observado da
relocation Xpsb para93d0; não houve modificação de evidência para obter PASS.

## INFERRED / UNKNOWN / BLOCKED

INFERRED: a classe representa uma operação ligada à entrega de estado/endereço
de textura, consistente com seus produtores. UNKNOWN: consumo físico desses
operandos e significado executado pela GPU. BLOCKED: programa PDS completo;
07000345 eAF continuam ENOTSUP. Primeiro PC permanece UNKNOWN e não foi
investigado nem usado como bloqueio nesta rodada.

Um replay adicional do mesmo IR válido identificou um criador R efetivamente
executado: ELF1c3c98 inicializa os14 registros R emctx+38; o count é fixado
em14 por1c3d2a. O store T207f9e ocorre depois. Isso refina a lista estática de
criadores, mas não recupera a ordenação final. Em `append-order-02`, os campos
`descriptor_words_final` são snapshots posteriores à compilação, NÃO os IDs
no instante de criação. Não se usa esse snapshot para alegar ordem de append.
Próximo experimento mínimo: observar a lista original após1c3d2a e antes/depois
do alocador237b63 no mesmo prefix-col01. Ver `GHIDRA_HANDOFF.md`. Não inventar
R,R,T nem considerar a ordem textual do IR uma prova.

ADMISSION/gates preservados. Zero SSH, GPU, MMIO, kicks, módulos ou mudanças
no Mini12. O DRI não foi instalado no sistema. `TEST_RESULTS.json` distingue
PASS de software da classificação parcial da recuperação semântica.
