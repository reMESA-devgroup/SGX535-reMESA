# Phase 9 — contratos de tarefa PDS e exportação VS

Esta entrega continua o checkpoint `phase9-pds-integration-20261018`. Foram
verificados os hashes de seus 1.081 artefatos e dos 258 fontes/documentos antes
de qualquer edição. As alterações estão no mesmo worktree; o novo snapshot,
patch, evidências e resultados ficam em
`/home/gama/sgx535-offline/phase9-execution-contract-20261018`.

O avanço implementado inclui um campo CPU de controle nas receitas DATA,
roteamento lógico completo dos inputs de tarefa, planejamento de exportação
VS usado pelo emissor do candidato e um contrato integrado validado pelos
callbacks Gallium. A geração de CODE PDS executável continua bloqueada pelas
semânticas físicas ausentes. Nenhum retorno ENOTSUP foi substituído por uma
qualificação fictícia de hardware.

## Evidência funcional nova

**CONFIRMED — fluxo estático da CPU.** No Xpsb de SHA256
`da531587b1ec59fe433fa20ddd2e691b2f8b7fb35bb82525d4db99259c5e571f`:

- ELF `0x893e` lê o uint32 de metadata `+0x18`;
- ELF `0x8948` desloca esse valor 27 bits à esquerda;
- ELF `0x894b` escreve o resultado em DATA `+4`, sem outros termos;
- o produtor examinado escreve metadata[6]=0 no caminho dinâmico e 1 no
  caminho de referência estática;
- ELF `0x8941` escreve o literal `0x20` em DATA `+0x20`. Seu significado
  arquitetural permanece desconhecido e não foi implementado como recurso.

A relação numérica é `DATA[1] = uint32(metadata[6] << 27)`. O domínio sem perda
do shift seria 0..31; a API nova admite apenas os valores 0/1 observados nesse
produtor. A máscara de contribuição do writer é `0xf8000000`. Ela não prova
que esses bits sejam recursos, flags de término ou controles aceitos pela GPU.
Outro perfil OGL retido escreve um valor diferente em sua palavra correspondente;
o codec desta entrega está explicitamente associado ao writer CPU examinado.

O pacote `evidence/CPU-TASK-CONTRACT-EVIDENCE.json` registra hashes e loci, junto
dos dois disassemblies x86 delimitados de writer e produtor. O trecho do writer
termina antes da escrita de instruções GPU. A identidade foi vinculada ao alvo
pela sessão REA da raiz, fechada após a consulta; a análise focada reaproveitou
decompilação e evidências retidas e conferiu instruções x86 com llvm-objdump.
O catálogo do subagente não expunha REA, embora a ferramenta estivesse disponível
na raiz. Não houve instalação de ferramentas ou execução do binário alvo.

Essa consulta teve como pergunta o campo de controle de DATA. Não repetiu a
cadeia CPU→XHW→MMIO, busca de palavra PDS conhecida ou investigação de primeiro PC.

## Implementação PDS

**IMPLEMENTED_OFFLINE.** `remesa_pds_cpu_meta6_pack()` e `unpack()` compõem e
validam o campo numérico. A receita nova `REMESA_PDS_DATA_CPU_META6` armazena o
valor 0/1 e usa esse codec na construção original de DATA. Não há seleção
automática por estágio ou colocação automática em uma palavra de tarefa.

```text
cpu_meta6_pack(0) -> 0x00000000
cpu_meta6_pack(1) -> 0x08000000
outro valor     -> ERANGE, output preservado
unpack com bits fora do perfil -> EPROTO
```

`remesa_pds_prepare_task_pair()` resolve cada célula DATA numa transferência
tipada fornecida pelo chamador, de índice de célula para slot lógico. O plano
registra valor resolvido, endereço da célula, tipo, slot e vínculo USE exato.
Há cobertura completa, uma definição por célula e destinos únicos. A saída é
canônica por ordem de célula, independentemente da ordem dos pedidos.

Os 64 slots são um limite da estrutura host, sem correspondência presumida com
capacidade ou bancos DS da SGX. DATA maior continua aceita pelo construtor
anterior; o planejador novo retorna E2BIG. A transferência física DATA→DS e
seus campos de instrução continuam UNKNOWN.

`cpu_relocation_mask` marca somente os 19 bits do endereço USE já estabelecidos
no contrato de relocation da CPU. Literais e metadata6 têm máscara de relocation
zero, embora seus valores CPU estejam resolvidos. Isso evita tratar um campo
CPU conhecido como semântica arquitetural comprovada.

Ambos os estágios são validados antes da publicação. São rejeitados fontes
ausentes/duplicadas, destinos repetidos, índices e tamanhos inválidos, overflow,
aliases, estágio incorreto e vínculos stale. A revalidação compara os valores
com as fontes atuais e conserva DATA→DS, despacho e término como ENOTSUP.

A revisão encontrou e corrigiu um defeito novo: validar isoladamente um link
retido não garantia igualdade com o link atual. A validação agora exige entrada
e offset DATA atuais exatos; testes substituem somente o link por alternativas
autoconsistentes com outro banco ou offset e exigem ESTALE.

## Exportação VS

**IMPLEMENTED_OFFLINE.** `remesa_usse_vertex_export_plan()` acompanha os quatro
canais POSITION float32 até suas definições de entrada, atravessando aliases
semânticos. Cada rota inclui os índices das instruções/valores, dependências,
atribuição numérica explícita e offset no candidato. As máscaras de leitura e
escrita por classe descrevem índices do namespace do writer CPU, sem inferir
contagem de temporários ou bancos físicos da GPU.

O emissor `remesa_usse_vertex_candidate()` passou a consumir essas rotas.
Assim o plano participa da geração dos quatro registros numéricos existentes.
Os bytes e controles anteriores foram preservados. A comparação exportada com
o checkpoint verifica byte a byte VS, FS e DATA anteriores.

Banco físico de entrada, entrega de POSITION ao consumidor seguinte e término
permanecem ENOTSUP. O fato de um postprocessador CPU alterar um bit no último
registro não estabelece que esse bit seja END ou exportação. Nenhuma sequência
proprietária foi transplantada para preencher essa lacuna.

## Integração do contrato ao pipeline

**IMPLEMENTED_OFFLINE.** O pipeline host existente oferece agora:

- `prepare_contract`: resolve as receitas imutáveis, slots explícitos, entradas
  USE e rotas VS em um snapshot independente das fontes;
- `validate_contract`: verifica o snapshot contra o pipeline e os pedidos
  atuais, incluindo os erros e bloqueios declarados;
- `encode_contract`: valida o contrato completo e retorna ENOTSUP sem escrever
  bytes enquanto as regras de emissão/execução estiverem ausentes.

Os bloqueios tipados são DATA→DS, controles de tarefa, instruções PDS, entrada
VS física, exportação VS, término USSE e conclusão/publicação do novo pipeline.
Zerar um status ou remover um bloqueio produz erro de validação; não libera
execução. Os mecanismos anteriores de fence/retirement/readback permanecem no
seu caminho existente. `completion_error` descreve a qualificação do novo par
original, não a implementação desses mecanismos anteriores.

Os wrappers Gallium `prepare_shader_contract()` e `revalidate_shader_contract()`
checam o par VS/FS atualmente vinculado e as receitas/layouts atuais antes da
preparação. Falha de compilação mantém o par anterior e seu diagnóstico; rebind
invalida o contrato. Outputs não podem sobrepor contexto, screen, shaders,
metadata, receitas, DATA ou candidatos imutáveis.

O teste de `draw_vbo` com backend de gravação agora prepara e valida esse
contrato durante a preparação do shader. O candidato VS usado pela preparação
nativa também passa pelo planejador de rotas. O backend Linux não recebeu uma
permissão de execução nova, e a preparação host não foi habilitada como caminho
experimental por padrão.

## Validação

| Matriz | Resultado |
|---|---|
| Roteamento/task-control PDS e codec metadata6 | 8.742 verificações por variante nativa, UBSan e i386/QEMU; arquivo Mesa real também |
| Exportação VS, dependências e recursos numéricos | 65.723 verificações por variante; arquivo Mesa real também |
| Contrato integrado, imutabilidade e emissão bloqueada | 195 verificações por variante; arquivo Mesa real também |
| Gallium e preparação de draw | 68 verificações por variante, preservando as 65 anteriores; arquivo Mesa real também |
| Construtores NIR reais com validador upstream | 32 casos, preservando os 30 anteriores e acrescentando POSITION e alias VS |
| Regressão USSE/PDS | 377.452 verificações por variante, sem redução |
| DATA e pipeline anteriores | 10.315 e 207 verificações por variante; arquivo Mesa real também |
| Demais regressões | FS, programas, corpo maior, NIR/draw/caps, recursos, framebuffer, fences, staging e transporte interposto |
| Build Mesa 25.2.0 i386 | Arquivo estático e DRI, sem instalação |

Os resultados finais, comandos e logs estão em `RESULTS.json`. Testes negativos
cobrem entradas malformadas, controles desconhecidos, estágio inválido,
dependências, limites, overflow, estado incompleto, links alternativos stale,
adulteração dos planos e flags de qualificação falsas. A qualificação do arquivo
estático exige igualdade dos fontes staged, Ninja sem trabalho pendente e hash
estável do arquivo. O validador NIR upstream é ligado ao teste com asserts.

`VALIDATION-INCIDENTS.md` preserva as falhas de configuração de link/protótipo e
os resultados intermediários anteriores à revisão, sem usá-los como aprovação
da fonte final. O aviso prévio do linker DRI sobre LOAD RWX permanece.

## Estados e limites restantes

| Estado | Conclusão |
|---|---|
| CONFIRMED | Equações e colocações do writer CPU indicado, produtores 0/1 examinados e campos públicos do codec USE; escopo estático/software |
| IMPLEMENTED_OFFLINE | Codec restrito, DATA tipada, rotas lógicas, vínculos completos, rotas VS, contratos e integração Gallium com testes CPU |
| INFERRED | B+q conserva somente seu papel anterior de hipótese operacional; nenhuma hipótese nova foi usada como regra de execução |
| UNKNOWN | DS físico, aridade/consumo e significado completo das palavras de tarefa, término PDS/USSE, exportação física VS e primeiro PC |
| BLOCKED | CODE PDS executável original e execução do par VS/FS completo dependem desses consumidores/semânticas ausentes |

`remesa_pds_encode()` continua ENOTSUP após validar o plano porque não há regra
admitida de emissão completa. `encode_contract()` identifica os bloqueios antes
de chegar ao mesmo limite. O encoder USSE genérico continua recusando VS; o
corpo FS restrito de oito bytes segue disponível. Schema/cell kind não suportado,
perfil CPU numérico desconhecido e NIR fora dos perfis têm rejeições separadas.
Nenhuma flag do chamador converte UNKNOWN em CONFIRMED.

O próximo passo mínimo para a primeira regra de emissão PDS é obter evidência
independente do consumidor de task output: fontes DS, aridade e campos de
controle que ele interpreta. Os inputs originais, as relocations e as rotas
agora têm representação tipada e validada para alimentar esse lowering. A
exportação/término VS ainda precisa do seu consumidor físico correspondente.

ADMISSION permanece `0x1f`; gates e defaults de laboratório estão intactos.
Houve zero submissões GPU, zero conexões SSH e nenhuma instalação. O arquivo
`FIRST-PC-STATUS.json` não foi alterado, e a investigação continua inativa.
Nenhum contexto OpenGL 2.1 funcional ou execução GPU nova é declarado.
