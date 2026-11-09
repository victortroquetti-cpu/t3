# Instruções para agentes (Codex e outros)

As instruções do projeto estão no `CLAUDE.md`, na raiz. Leia o arquivo **inteiro** antes de mexer e siga as
regras dele: responder em português do Brasil, fazer só o que o dono pedir, não usar assets de outros jogos,
mudanças em arquivo da Valve com `#ifdef TROK`, números em cfg ou script.

Depois, na ordem que o `CLAUDE.md` indica:

1. `cs-source/HANDOFF.md`
2. `cs-source/design/` (modos, Surto, sistemas, UI, bots, lore)
3. `.claude/skills/source-mod/SKILL.md` e `references/`: referência do Source SDK, NextBot, rede e predição,
   conteúdo. É uma skill do Claude Code, mas é texto comum; leia como documentação.
4. `cs-source/tools/mcp/LEIA-ME.md`: o servidor MCP `trok` para abrir e testar o jogo.
