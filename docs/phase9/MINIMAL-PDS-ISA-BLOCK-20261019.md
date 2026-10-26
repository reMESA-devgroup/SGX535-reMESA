# PDS_BLOCKED_BY_ISA — menor caminho de CODE

Resultado: **zero instruções PDS semânticas originais geradas; zero bytes CODE**.
`remesa_pds_encode()` continua ENOTSUP. Nenhum arquivo de produção foi alterado
nesta rodada, e não foram adicionados wrappers, perfis CPU ou validadores ao
driver. O checkpoint de entrada já contém os componentes host independentes
implementáveis. A falta atual é uma regra ISA do consumidor, não outro contrato
de armazenamento ou relocation.

Pacote reproduzível: `/home/gama/sgx535-offline/phase9-minimal-pds-isa-20261019`.
O checkpoint anterior teve seus 1.142 artefatos selados e 265 fontes/documentos
verificados antes da investigação. Os testes desta rodada reutilizam os fontes
compilados idênticos. O patch incremental contém este relatório; o código novo
de análise está em `ISA_WITNESS.py`, fora do driver. Não é encoder ou decoder.

## A primeira dependência de emissão

O primeiro DWORD CODE dos construtores selecionados é `0x07000345`. A escrita
CPU desse valor e sua posição no programa são fatos estáticos reproduzíveis.
Não há uma definição independente admitida que o transforme na operação
semântica “enviar T0/T1/T2 ao consumidor USSE”. A emissão dessa operação depende,
antes de qualquer outra coisa, de identificar a classe/opcode, a porta de
destino, a quantidade/ordem das fontes e o modo de endereçamento.

**Menor bloqueio:** a regra de task output dessa primeira classe, começando pela
associação do destino numérico5 ao consumidor USSE e pela interpretação completa
dos seletores. Se esse destino for outro port, operação ou registrador, um
programa com os mesmos dados não despacha a tarefa pretendida. A regra de término
também permanece ausente e impedirá o programa completo mesmo após esse passo.

Copiar o primeiro DWORD e acrescentar o literal final histórico seria um
programa fixo transplantado. Reproduzir a decomposição numérica com shifts e OR
não demonstra que as entradas do encoder sejam operações semânticas corretas.
Por isso não foi criado um falso caminho EXECUTABLE_CANDIDATE_OFFLINE.

## Matriz mínima de campos e alternativas

| Campo/operação | Observação | Hipótese e alternativas | Confiança e discriminação independente |
|---|---|---|---|
| DATA→DS | A CPU prepara regiões DATA e calcula suas extensões | Preload automático antes de CODE; ou transferências/estado adicionais | UNKNOWN físico. Uma descrição aplicável do preload deve indicar banco, ordem e instante; não inventar uma instrução LOAD para preencher a lacuna |
| Largura/endian | Os writers examinados gravam DWORDs x86 little-endian e avançam quatro bytes | Instruções PDS de32bits; framing/controle adicionais não demonstrados | CONFIRMED CPU; INFERRED largura ISA. Requer formato de instrução independente, não somente tamanho do store CPU |
| Alinhamento | Allocations/subprogramas seguem políticas CPU e descritores próprios | Alinhamento do bloco pode diferir do alinhamento de instrução | UNKNOWN ISA; requer regra de fetch/formato. O primeiro PC não foi investigado |
| Classe/destino | O perfil numérico selecionado admite low nibble5 e high group0 | Task output USSE; outro port, opcode ou destino | UNKNOWN ISA. Decoder aplicável ou trace da porta deve mostrar enqueue USSE com essas fontes |
| Predicate/modos | O campo numérico de três bits assume7 no perfil | Sempre; predicado ou outro modo com condição satisfeita no caso histórico | UNKNOWN ISA. Exige tabela de condições ou observação com estado de predicado controlado |
| Índices/swizzle | A partição candidata contém A, B e quatro seletores de dois bits | Índices de pares versus escalares; bits de modo misturados ao índice | UNKNOWN ISA. Fontes não-zero e valores sentinela, com observação das fontes físicas, distinguem as projeções |
| Aridade | A CPU inicializa três valores de tarefa em DATA0/1/8 | Três palavras; quarta leitura explícita/implícita; controle adicional | UNKNOWN ISA. Contar acessos/palavras na porta; a imagem final não dá essa contagem |
| Endereço USE | O contrato CPU lower19 já liga bank e offset à entrada USE | O consumidor da instrução interpreta T0 nessa posição/ordem | CONFIRMED CPU; UNKNOWN consumo físico. Requer tarefa decodificada e comparação com o estado USE correspondente |
| Fim/sync | Um DWORD numérico é CPU-final em vários construtores | HALT; wait/drain/controle que pode ler estado implícito | UNKNOWN ISA. Exige regra de término e observação de issue/drain/retirement, separadas da conclusão do serviço/readback |
| Bits não variados | Há bits constantes no corpus selecionado | Reservados; opcode; flags fixas de um perfil restrito | Apenas constância CPU confirmada. Uma tabela válida deve separar constantes do opcode, variáveis e bits realmente reservados |

A partição **matemática existente**, ainda não uma tabela ISA, é:

```text
31..27: 0       26..24: número7
23..18: A       17..13: B
12..11: S0      10..9: S1      8..7: S2      6..5: S3
4: 0           3..0: número5
```

No DWORD selecionado, A=B=0 e S=(0,1,2,2). Os nomes “índice”, “swizzle”,
“predicate” e “port” são hipóteses para esses campos. O zero do bit4 não prova
que ele seja reservado. Variações em outras classes CPU não estabelecem
automaticamente a mesma semântica nesta classe. As máscaras hipotéticas não
foram promovidas a enum de opcode, operação ou encoder de CODE.

## Discriminante executável e limite dos históricos

`ISA_WITNESS.py` compara duas hipóteses explícitas, sem produzir palavras:

```text
Hpair:   index = 2*source + half
Hscalar: index =   source + half
```

Com A=B=0, ambas fornecem exatamente os mesmos índices para qualquer DATA.
Portanto, mesmo um readback perfeito desse caso não discrimina a unidade do
índice. Com A=1, B=0, as três primeiras fontes previstas são respectivamente
DS0[2], DS0[3], DS1[0] e DS0[1], DS0[2], DS1[0]. O padrão independente
DS0[1]=1, demais células=0 produz vetores(0,0,0) e(1,0,0). Isso define uma
diferença falsificável caso se obtenha uma observação da porta/fonte aplicável;
não autoriza codificar esses operandos como se uma das hipóteses estivesse
confirmada. Os cinco testes do script checam esse raciocínio, não a ISA.

Ainda no modelo candidato, o quarto seletor repete o endereço do terceiro.
Uma quarta leitura pode existir sem introduzir um endereço novo. Variação de
conteúdo e igualdade de readsets não substituem observação da contagem de
acessos/palavras. Predicate, destino e término não são resolvidos pelo witness.

A auditoria de primários em `evidence/historical/` verificou os18 originais FIRE3,
a cápsula/readback de120pixels e a cápsula/readback de256pixels do square.
FIRE3 altera o corpo USSE mantendo PDS fixo; square altera geometria/TA.
As cápsulas registram resultado, fase, eventos e imagem, não DS ou decodificação
PDS. O replay de journal é explicitamente INTERPOSED_CPU_ONLY. Esses casos não
satisfazem o critério de dois inputs semânticos e previsões CODE independentes.

## Evidência externa e CPU focada

As28 consultas públicas delimitadas admitiram zero campos ISA novos. A
[documentação oficial Mesa CSBGEN](https://docs.mesa3d.org/drivers/powervr/csbgen.html)
tem escopo Rogue. O nome PDS não estabelece compatibilidade com SGX535. Afirmações
recentes em fórum originadas deste projeto foram excluídas como corroboração.

A [patente Imagination US8046761B2](https://patents.google.com/patent/US8046761B2/en)
separa a liberação de thread e output, sem estabelecer campos PDS SGX535. A
releitura de término é da mesma família já conhecida, não uma segunda fonte ISA.
A menção histórica a preload é uma hipótese útil para evitar inventar um LOAD;
a citação de manual em um fórum não foi promovida a definição admitida de bits.

A consulta REA nova examinou somente `Xpsb_pds_get_num_constants`, Evidence ID
`ev_776c5fd809bf93b35db72f2c1a291a19be450a6cec5f15c8d163e1426fdc4f1d`, vinculada
ao Xpsb de hash `da531587b1ec59fe433fa20ddd2e691b2f8b7fb35bb82525d4db99259c5e571f`.
Ela calcula uma extensão numérica a partir de dois contadores e não efetua uma
transferência DATA→DS nem identifica as leituras de instruções. O cálculo
intercalado da CPU não fecha o consumidor físico. A sessão foi fechada; nenhum
binário alvo foi executado. Não foram extraídas sequências shader.

## Validação e próximo passo

`qualification/` reproduz a matriz offline do checkpoint: nativo, UBSan,
i386/QEMU, regressões anteriores, NIR/Gallium e os18 jobs do arquivo Mesa real.
Os targets estático e DRI Mesa25.2.0 são conferidos pelo Ninja. Os campos ISA
novos não têm testes de encode/decode porque nenhum campo foi admitido: testar
duas funções que compartilham a mesma hipótese não forneceria uma referência.
`RESULTS.json` separa PASS_OFFLINE das regressões do resultado PDS_BLOCKED_BY_ISA.

A próxima evidência de maior valor é **uma referência independente aplicável à
SGX535 que decodifique task output com fontes não-zero e seu término**. Deve
explicar destino, fontes/ordem/aridade, condições e bits reservados, e prever
dois casos semânticos distintos. Uma captura passiva com valores/acessos DS,
palavras na porta USSE e transição de término poderia distinguir os modelos;
não existe tal observação nos históricos auditados. Nenhuma experiência GPU foi
preparada para execução ou realizada nesta rodada.

ADMISSION, gates e defaults permaneceram intactos. Zero GPU, SSH e DDK vazado.
Primeiro PC permanece UNKNOWN com investigação inativa. Nenhuma execução PDS
ou OpenGL funcional foi declarada.
