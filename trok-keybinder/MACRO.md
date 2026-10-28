# Trok KeyBinder — lembrete da aba Macro

A aba **Macro** existe no menu, mas ainda não faz nada. Ela só mostra "em breve" e as regras abaixo. Este arquivo guarda o que foi pedido para quando ela for feita.

## O que o macro grava

O jogador aperta **Gravar** e faz as coisas normalmente. O mod grava:

- **abrir o chat** e **o que foi escrito**, incluindo comandos;
- **interações com menus**:
  - **Diálogos do servidor:** a lista, a escolha, os botões e o texto digitado na caixa.
  - **Menus do SA-MP** (ShowMenuForPlayer) e **textdraws clicáveis** (SelectTextDraw).

O jogador aperta **Parar** e o mod salva o macro, com um nome, no preset do servidor (como as binds). Depois ele roda como uma bind: por tecla, pelo menu circular ou por uma condição do Automático.

## Item de menu pelo ID, não pela posição

Ao escolher um item num menu, o macro grava **qual menu era e o ID do item**, não a linha nem o lugar na tela.

Na hora de rodar, o mod:

1. espera o menu certo abrir;
2. **procura o item nesse menu**;
3. se achar, escolhe; se não achar (o servidor mudou o menu, o item sumiu), **para o macro** e avisa.

Como identificar o menu e o item:

- **Diálogo:** o `dialogid` que o servidor manda no `ShowPlayerDialog`. Os itens são as linhas da lista; procura pelo texto da linha, sem as cores `{RRGGBB}` (o mesmo `Normalize` das mensagens no chat). A posição só vale como desempate.
- **Menu do SA-MP:** o título do menu e o texto do item.
- **Textdraw:** o ID do textdraw. Se o servidor recriar com outro ID, usa o texto e a posição como reserva.

## O que o macro nunca grava

**Nada de jogabilidade.** Andar, correr, dirigir, mirar, atirar, ir até um lugar ou "andar até X, digitar Y e ir até Z" é **proibido no servidor**.

O macro grava só o chat e os menus. Teclas de movimento, mouse da câmera e estado do personagem ficam de fora da gravação, mesmo com o jogador se mexendo enquanto grava.

## Cuidados quando for implementar

- Tudo passa pelo anti-flood do mod: a mesma fila das binds, com o mesmo intervalo da aba Geral.
- O macro para se aparecer um diálogo que não estava na gravação, se o jogador morrer ou se o chat for aberto à mão.
- Grava os tempos entre os passos, mas nunca roda mais rápido que o anti-flood.
- No arquivo, os passos ficam legíveis, como as binds (`[macro]`, `passo=chat|/comando`, `passo=dialogo|1234|Item`...).
