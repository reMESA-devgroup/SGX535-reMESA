# Retomada do driver — 18/10/2026

O encoder público USSE passa a produzir o corpo original de fragmento constante
de oito bytes para verde e magenta. A criação de shader Gallium aceita também
expressões constantes NIR com `iand`, `ior`, `ixor`, `inot` e `bcsel`. Essas
expressões são avaliadas na CPU e reduzidas aos perfis de cor existentes;
nenhuma instrução ALU nova da SGX foi presumida.

As alterações estão no worktree `phase9/driver-resume-20261018`. A implementação
parte da integração de 14/10 com suas alterações locais anteriores preservadas.
O repositório principal não foi usado como destino das alterações. O snapshot
anterior de 236 arquivos e o patch específico desta entrega estão em
`/home/gama/sgx535-offline/driver-resume-20261018`.

## Por que os encoders retornavam ENOTSUP

`remesa_usse_encode()` validava o programa semântico e retornava `ENOTSUP`
incondicionalmente. Havia, separadamente, um compositor original de campos
restrito em `remesa_fragment_constant_prepare()`, já usado pelas imagens de
shader. Faltava conectá-lo à API pública de codificação e aos diagnósticos do
shader. A conexão agora está implementada; não foi escrito um encoder geral
da ISA a partir de semânticas desconhecidas.

`remesa_pds_encode()` valida o programa e seu plano de bindings, mas não contém
um gerador completo de instruções e palavras de tarefa PDS. Seu retorno final
continua sendo `ENOTSUP`. A presença de endereço USE e de uma relocation de
DATA não define os controles de despacho, a origem dos operandos ou o consumo
da tarefa. Isso é uma limitação de geração de programa, separada de ownership,
serialização, limites e fixups já implementados.

| Camada | Implementação existente preservada | Resultado desta entrega / limite restante |
|---|---|---|
| API Gallium/NIR | `create_fs_state`, `create_vs_state`, ownership e liberação do NIR, tokens imutáveis | Mais formas constantes aceitas; nenhum novo cap OpenGL anunciado |
| Validação | Perfis de posição e cor, SSA finita, estágios, bounds, dependências e armazenamento canônico | Bitops uint32 e condição booleana constante de 1 bit; larguras, swizzles, referências inválidas e ALU flutuante rejeitados |
| Armazenamento | `remesa_shader_storage`, imagens, staging VS/FS, cópias e vida útil após destruição dos shaders | O caminho da imagem chama o mesmo encoder público; tamanho e forma anteriores preservados |
| Relocations | `remesa_pds_prepare`, bindings/epochs/ownership, vínculos USE e fixups de 19 bits em DATA da CPU | Reutilizados sem mudanças; não constituem programa PDS executável |
| Instruções USSE | Compositor original de campos para o fragmento constante empacotado | API pública emite 8 bytes para `0xffff00ff` e `0xff00ff00`; VS e demais perfis retornam `ENOTSUP` |
| Programa PDS / pipeline | Planos semânticos, previews numéricos e preparação de draw | Encoder PDS e qualificação da execução completa continuam `ENOTSUP` |
| Recursos e execução | Recursos, transfers, framebuffer, fences e backend de laboratório existentes | Nenhuma reescrita, abertura de gate, instalação ou submissão GPU |

## Comportamento verificável

Para um FS verde ou magenta admitido:

```text
remesa_usse_encoded_size(program, &bytes)       -> 0, bytes = 8
remesa_usse_encode(program, output, cap, &size) -> 0, size = 8 (cap >= 8)
shader_program_report.binary_error            = 0
shader_program_report.binary_bytes            = 8
shader_program_report.launch_error            = ENOTSUP
pipeline.binary_error                         = ENOTSUP
```

O encoder preserva todos os outputs em caso de erro e não modifica bytes após
o corpo emitido. Buffer curto retorna `ENOSPC`; ponteiros nulos incompatíveis,
overlap e wrap de endereços retornam `EINVAL`. A consulta de tamanho não emite
bytes. A API não recebe registradores físicos ou controles de execução livres.

Amarelo continua sendo um perfil semântico reconhecido, mas o encoder público
empacotado retorna `ENOTSUP`. O candidato maior já existente no armazenamento
permanece disponível no mesmo caminho anterior. O VS de quatro movimentos
continua candidato numérico, sem promoção a programa executável.

`binary_error = 0` significa somente que o corpo restrito pode ser codificado.
O status semântico, `launch_error`, a validação de execução da imagem e os gates
de admissão continuam separados. Os oito bytes não são uma qualificação nova
de hardware nem um pipeline independente do carrier histórico.

O reconhecimento NIR mantém a estrutura recebida intacta e exige definições
anteriores no mesmo bloco, até 16 valores constantes. Todos os operandos,
inclusive o ramo não selecionado de `bcsel`, entram na contabilidade de
dependências. Valores sem uso, ciclos, referências futuras, recursos,
side effects e resultados fora dos perfis admitidos continuam rejeitados.

## Proveniência

Foi reutilizado `remesa_fragment_body.c` do baseline, sem alterar seus campos,
opcodes ou controles. Esse módulo compõe os campos matematicamente e conserva
o status `REMESA_FRAGMENT_BODY_CPU_CANDIDATE`. Nenhuma sequência foi obtida de
DDK ou shader proprietário durante este desenvolvimento. Os testes comparam
cores semânticas, roundtrips, limites e integridade; não incorporam templates
de instruções proprietárias.

Os resultados históricos de rasterização pertencem às respectivas execuções
e carriers. Não qualificam, por associação, o par atual VS/PDS original.

## Validação offline

Os comandos, hashes dos inputs, logs e resultados estão no diretório de entrega.
São fixtures da CPU, Mesa NIR e backends de gravação/interposição. Nenhum teste
abre a GPU ou submete comandos reais.

| Verificação | Resultado |
|---|---|
| Novo NIR → IR → encoder → callbacks FS, incluindo alias do objeto completo | PASS, 163 verificações por variante nativa, UBSan e i386/QEMU |
| Regressão USSE/PDS, validação e fixups | PASS, 377452 verificações por variante nativa, UBSan e i386/QEMU |
| Programas semânticos e callbacks existentes | PASS, 420 verificações por variante nativa, UBSan e i386/QEMU |
| Testes com o arquivo estático real do Mesa i386 | PASS: novo encoder (163), programas (420), staging/ownership (1268), handoff de draw (227), constante empacotada (295), corpo candidato maior (531), integração NIR/draw |
| Construtores reais NIR, com validador upstream e asserts | PASS, 30 casos no Mesa i386, incluindo as seis novas expressões constantes |
| Regressão NIR, integração de draw e caps existentes | PASS, nativo, UBSan e i386/QEMU |
| Transfers, framebuffer e sincronização | PASS, nativo e UBSan, incluindo a política existente de fences |
| Build Mesa 25.2.0 i386, Meson/Ninja | PASS: `libremesa_loader.a` e `libgallium-25.2.0.so`, sem instalação |

Os testes do arquivo estático verificam igualdade byte a byte entre os fontes
do worktree e os fontes staged, ausência de rebuild pendente no Ninja e hashes
do arquivo antes e depois da execução. Os objetos e fontes staged anteriores
foram copiados para `mesa-before` antes do rebuild. O linker ainda informa o
segmento LOAD RWX já presente na integração; esta entrega não o corrige.

O Mesa release omite seu validador de debug. O teste dos construtores compila
o `nir_validate.c` upstream com asserts e o liga ao executável de teste, usando
a máscara `NIR_DEBUG_EXTENDED_VALIDATION`. O arquivo estático do driver é o
mesmo testado nos demais casos. Falhas intermediárias de configuração do link,
o snapshot inicial incompleto e a revisão de alias estão registrados em
`VALIDATION-INCIDENTS.md` no diretório de entrega; esses logs foram preservados.

## Continuidade

O próximo passo para um contexto OpenGL funcional é completar o caminho
original de geração e ligação VS/PDS e integrar sua qualificação ao backend
existente. Em paralelo, a API de fragmento agora oferece uma base testável para
o subconjunto constante, com um único gerador de bytes para inspeção e staging.
Uma nova execução de hardware exige autorização específica e permanece limitada
a uma única submissão, sem repetição automática.

A investigação do primeiro PC foi encerrada como tarefa ativa:
[`FIRST-PC-STATUS.json`](FIRST-PC-STATUS.json) registra `first_pc = UNKNOWN`
e `B+q` como hipótese operacional. Os artefatos anteriores ficam preservados.
Essa questão só será reaberta com evidência nova ou bloqueio funcional concreto.
ADMISSION e a política de submissão não foram alterados.
