# Vaio P: primeira coleta de identidade

Esta etapa identifica a máquina para preparar uma futura validação do reMESA.
Ela não instala driver, altera boot, carrega/remove módulos ou executa GPU.
O modelo "Vaio P" sozinho não comprova PCI, revisão SGX, MMU, painel ou kernel
compatíveis com o Dell Mini 12 já testado.

Com Python 3, extraia o pacote em uma pasta e execute sem sudo:

```sh
python3 collect_platform_identity.py --output vaio-p-identidade.json
```

No repositório, o mesmo comando é:

```sh
python3 tools/remesa/collect_platform_identity.py --output vaio-p-identidade.json
```

O coletor lê distribuição/kernel/CPU/RAM, modelo e BIOS sem números de série,
dispositivos PCI gráficos, driver vinculado e nomes de cards DRM. Não abre
`/dev/dri`, arquivos PCI `resource*` nem registradores. Não coleta hostname,
UUID, rede ou logs completos. O único arquivo criado é o JSON pedido; um
arquivo existente não é sobrescrito. Campos inacessíveis ficam `null`.

Envie o JSON para comparação. Um candidato PCI `8086:8108` ainda não comprova
SGX535 rev121: a revisão PCI é diferente da revisão interna SGX. Parâmetros
ausentes também não comprovam controles N/N. A coleta não certifica aceleração.

Mantenha o sistema e o boot atuais. O candidato privado em desenvolvimento
ainda não está fisicamente qualificado para o Vaio; não há instalação de módulo
ou teste de renderização neste pacote. Um kernel diferente do alvo antiX
5.10.240 precisa de build e revisão próprios antes de qualquer instalação.
