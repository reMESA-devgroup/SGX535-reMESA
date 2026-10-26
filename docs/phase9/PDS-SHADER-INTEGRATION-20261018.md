# Phase 9 — construção original de DATA PDS e integração VS/FS

Esta entrega acrescenta código ao checkpoint `driver-resume-20261018`: um
construtor de DATA original, um compilador/ligador de recursos na CPU para o
par mínimo VS/FS e sua conexão aos shaders vinculados no Gallium. CODE PDS
executável continua indisponível. Não houve instalação ou execução na GPU.

O worktree permanece em `/home/gama/sgx535-worktrees/driver-resume-20261018`.
O patch incremental, fontes anteriores, logs, binários de teste e manifest
ficam em `/home/gama/sgx535-offline/phase9-pds-integration-20261018`.
O baseline incremental é o checkpoint validado de 239 arquivos; o snapshot
desta rodada também inclui documentos e totaliza 254 arquivos. O worktree
`render-pass-20261009` foi consultado como referência histórica e preservado.

## Funcionalidades implementadas

`remesa_pds_build_host_data_pair()` constrói os buffers DATA de VS e FS a partir
de receitas originais. Cada célula cobre quatro bytes little endian e possui
um tipo explícito:

- `REMESA_PDS_DATA_LITERAL32`: valor literal da CPU, sem semântica de tarefa;
- `REMESA_PDS_DATA_USE_ENTRY_LOWER19`: uma relocation por estágio, exatamente
  na posição do vínculo USE validado. O valor original fornece apenas os
  13 bits superiores opacos; os 19 inferiores vêm do codec existente.

As duas receitas, buffers, identidades e vínculos são validados antes da
primeira escrita. O construtor rejeita tipos desconhecidos, relocations
ausentes ou duplicadas, bits inferiores fornecidos pelo chamador, tamanhos
inexatos, alinhamento inválido do vínculo, overflow, aliases e metadados stale.
Qualquer erro preserva os dois buffers. O verificador correspondente compara
todos os bytes, incluindo literais fora da relocation. CODE não é lido nem
escrito. DATA e CODE podem usar o mesmo bloco ou alocações distintas.

`remesa_shader_host_pipeline_create()` reúne em uma operação transacional:

1. validação do par semântico derivado de NIR;
2. geração do candidato VS existente com quatro registros de movimento e
   atribuição numérica explícita, sem selecionar bancos físicos por padrão;
3. geração do FS constante existente e obtenção dos comprimentos efetivos;
4. divisão dos blocos DATA/CODE pelo alinhamento solicitado pelo chamador;
5. ligação dos comprimentos exatos VS/FS aos endereços USE, bancos e DATA;
6. construção original de DATA e cópia para `remesa_shader_storage`;
7. publicação de um objeto que possui a imagem, receitas e vínculos imutáveis.

Falhas de alocação liberam todos os temporários. O objeto sobrevive à destruição
do NIR, dos shaders Gallium e das receitas de origem. A inspeção expõe vistas
constantes; a revalidação compara os programas, mapeamento, receitas e bindings
atuais, sem usar apenas hashes como prova de identidade. O limite DATA por
estágio é o limite de armazenamento host existente de 4096 bytes.

O FS verde/magenta permanece com oito bytes. O perfil amarelo conserva o
candidato maior de 56 bytes do checkpoint, sem ganhar suporte no encoder
empacotado. O candidato VS tem 32 bytes; esses tamanhos são do serializador
original, não uma afirmação nova sobre fetch ou instruções do silício.

## Ligação Gallium e erros de compilação

`remesa_context_prepare_host_shader_pipeline()` obtém o par atualmente vinculado
e chama o mesmo construtor. A revalidação detecta rebind, alteração de receita
ou metadados de mapeamento. A especialização de cor por UBO permanece no caminho
sincronizado de draw existente e não é implicitamente realizada por essa API.

`remesa_context_shader_compile_status()` conserva o último resultado de criação
por estágio, o número de tentativas, a disponibilidade/tamanho do corpo e o
erro de lançamento. Assim, um callback posterior que zera `context_error`
não apaga a causa de uma compilação recusada. Os callbacks de criação já
reportavam erros transitórios no checkpoint; essa implementação acrescenta
persistência e separação por estágio. Não modifica o comportamento desse erro
transitório nem vincula automaticamente um shader recém-criado.

O teste de integração usa `draw_vbo` real com backend de gravação. Sua função
de preparação chama o novo construtor a partir do snapshot VS/FS recebido.
Uma receita FS inválida falha antes de maps/submit; corrigir a receita permite
o mesmo fluxo existente de recursos, framebuffer e fences. O teste não troca
o backend Linux nem transforma um PASS de gravação em execução GPU.

Nenhuma linha de `remesa_linux`, `remesa_draw`, framebuffer, transferências,
fences, UAPI, gates ou admissão foi reimplementada para obter esse resultado.

## O que permanece ENOTSUP

| Operação | Estado e motivo |
|---|---|
| `remesa_pds_encode()` após validação | Falta contrato completo para instruções, roteamento DS, consumo de tarefa e controles de execução. Nenhum CODE é emitido. |
| `remesa_pds_validate()` com schema/reserved/contagem incompatível | Rejeição do formato de plano host, separada de geração de instruções. |
| Preview de descritor com CODE/DATA em alocações diferentes | A projeção numérica estreita requer bloco contíguo; o construtor DATA não tem essa limitação. |
| Preview/decode de instrução fora das famílias numéricas existentes | Rejeição do subconjunto `ANALYSIS_ONLY`; condições 3/7 não ganharam interpretação arquitetural. |
| Receita DATA com tipo desconhecido | Rejeição de compatibilidade do schema original; os dois tipos declarados estão implementados. |
| Transferência DATA → DS e palavras completas de tarefa | UNKNOWN. Literais opacos e relocation lower19 não fornecem essa semântica. |
| Encoder USSE para VS ou FS fora do perfil empacotado | Mantém o limite do checkpoint. O VS numérico e o FS maior continuam candidatos da CPU. |
| Qualificação da imagem/pipeline para execução | ENOTSUP; `pds_code_error` e `execution_error` continuam explícitos na inspeção. |

O alinhamento de CODE desta API é política host fornecida no pedido. O codec
USE reutiliza o contrato público do kernel em `remesa_program_registers.c`.
As identidades/VA fornecidas pelo chamador continuam simbólicas; nenhuma dessas
funções autentica GEM, MMU, cache, lease de banco ou publicação para a GPU.

## Evidência e clean-room

**CONFIRMED na CPU:** composição little endian, preservação dos bits superiores,
relocation lower19, separação de intervalos, alinhamento solicitado, limites,
ownership, rollback e fluxo de callbacks são reproduzidos pelos testes.

**UNKNOWN no hardware:** opcode/execução PDS, associação DATA→DS, número e
significado das palavras de tarefa, saída/terminação VS e execução do pipeline
original completo. O código não promove campos de preview a instruções.

A representação PDS disponível em `remesa_pds_candidate.h` tem flags de fetch,
semântica e aridade desconhecidas. Os inventários locais
`independent-encoding-evidence-20261009.json` e
`independent-program-registers.md` distinguem esses limites dos codecs MMIO.
Não foi encontrado um emitter completo admitido que pudesse ser conectado à
API. A revisão desta rodada usou código e índices locais; não extraiu programas
de `Xpsb.so`, `psb_dri.so` ou DDKs, nem reutilizou bytes de shaders históricos.

Esta descrição atualiza os escopos dos documentos antigos que ainda diziam
que ambos os emitters retornavam ENOTSUP incondicionalmente: o corpo FS restrito
já funciona desde o checkpoint anterior. Os documentos e receipts históricos
não foram reescritos para aparentar resultados novos.

## Validação e reprodução

| Matriz | Resultado final |
|---|---|
| DATA PDS original | PASS, 10.315 verificações em nativo, UBSan e i386/QEMU; também no arquivo estático Mesa |
| Pipeline host VS/FS/DATA | PASS, 207 verificações nas três variantes e no arquivo estático Mesa |
| Diagnósticos e draw Gallium | PASS, 65 verificações nas três variantes e no arquivo estático Mesa |
| Regressão USSE/PDS do checkpoint | PASS, 377.452 verificações por variante, sem redução da cobertura |
| FS restrito e programas existentes | PASS, 163 e 420 verificações por variante; também no arquivo estático Mesa |
| Construtores NIR reais + validador upstream | PASS, 30 casos no arquivo estático Mesa i386 |
| Armazenamento e integração existentes no Mesa | PASS: 1.268 verificações de staging/ownership, 227 de handoff, 295 de FS empacotado, 531 do corpo maior, integração de draw, 13 cenários de backing DATA e 986 verificações de transporte interposto |
| Regressão NIR, draw e caps | PASS nas variantes nativa, UBSan e i386/QEMU |
| Transfers/framebuffer/fences | PASS, nativo e UBSan |
| Mesa 25.2.0 i386 | PASS, arquivo estático e DRI recompilados |
| Artefatos originais exportados | PASS, cinco arquivos idênticos entre execução nativa e Mesa i386 |

`RESULTS.json` no diretório de entrega consolida a matriz final. Cada qualificação
retém comando de compilação, saída e variante. Os testes contra o arquivo
estático real verificam fontes staged idênticas, ausência de rebuild pendente
no Ninja e o hash do arquivo antes/depois. O Mesa 25.2.0 i386 é recompilado em
arquivo estático e DRI, sem instalação. O aviso prévio do linker sobre LOAD RWX
permanece uma limitação de build separada.

Os casos novos verificam bytes DATA, guards de CODE/padding, layouts separados,
relocations, stale bindings, tamanho exato, aliases/wrap e falha em cada ponto
de alocação. A revisão acrescentou proteção do objeto de imagem completo e dos
objetos Gallium contra alias das novas saídas. `VALIDATION-INCIDENTS.md` registra
os receipts intermediários substituídos por matrizes finais congeladas e a
execução duplicada abortada; nenhum receipt anterior foi editado para parecer
uma validação da versão final.

`implementation.patch` é incremental em relação a `BASELINE.json`. A aplicação
do patch é verificada numa cópia do snapshot. `REPRODUCE.py` permite reaplicar
as qualificações offline com os caminhos e bibliotecas preservados.
`examples/` contém DATA original e candidatos VS/FS produzidos duas vezes pela
fixture pública do novo teste, com comparação byte a byte. Não contém CODE PDS
nem programa GPU executável, e não é um pacote de submissão.

## Próximo passo rumo ao OpenGL 2.1

O próximo componente necessário do pipeline original é um lowering PDS que
consuma essas receitas/vínculos e produza despacho com contrato verificável
de DATA→DS, task output e término, acompanhado do contrato de export VS. A
integração agora recebe programas e dados originais completos na CPU; não
precisa aceitar snapshots históricos como fonte para DATA.

O contexto OpenGL continua barrado pelos cinco grupos de contratos existentes
em `remesa_dri_missing_contracts()` (`0x1f`). Resolver o pipeline mínimo ainda
não fornece texturas, regras de rasterização, blend/depth/stencil ou preservação
de COLOR exigidos pelo baseline do state tracker. Nenhum cap ou versão GL foi
alterado para forçar criação de contexto.

`FIRST-PC-STATUS.json` permanece intacto: investigação inativa, primeiro PC
UNKNOWN e hipótese operacional B+q. Esta rodada não dependeu de reabrir essa
questão. ADMISSION, gates e habilitação padrão do laboratório permanecem
inalterados. Houve zero submissões GPU e zero conexões SSH.
