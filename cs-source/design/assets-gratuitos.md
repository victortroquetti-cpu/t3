# Assets gratuitos: o que serve para o Trok

Pergunta do dono (09/11/2026): assets da Unreal ou de outros bancos gratuitos são úteis?
Resposta curta: **sim; o trabalho maior é converter para o Source 1.**

## Fontes

| Fonte | Licença | Serve? |
|---|---|---|
| Poly Haven (texturas, HDRIs, modelos) | CC0 | **sim**, sem restrição |
| ambientCG (materiais) | CC0 | **sim** |
| Kenney (modelos e sons estilizados) | CC0 | sim, se combinar com o visual |
| Freesound | CC0, CC-BY (dar crédito) ou CC-BY-NC | sim |
| Sketchfab | varia por item (CC-BY, CC0, licenças de loja) | sim |
| Mixamo (animações e personagens) | uso livre em projetos | **sim** para animações; precisa redirecionar para o esqueleto do Source |
| Fab / antigo Unreal Marketplace | depende do item | sim |

## Conversão: o Source 1 é de 2004

- **Texturas:** o que mais rende. Convertem para VTF (VTFEdit) direto. O Source 1 não tem PBR
  de fábrica: dá para usar só a cor e o normal map, ou instalar um shader PBR para o SDK 2013
  (existem projetos abertos).
- **Modelos:** quase sempre pesados demais (modelos de Unreal e fotogrametria têm centenas de
  milhares de triângulos). Precisam de redução (decimate/retopologia no Blender), UV e
  materiais refeitos, e compilar com `studiomdl` (Blender Source Tools). Props simples
  (caixas, barris, grades) compensam; personagens e armas dão muito trabalho.
- **Animações:** Mixamo é ótimo para referência e para animações de jogador, mas o Source usa o
  esqueleto ValveBiped: é preciso redirecionar (retarget) no Blender. Animações de arma em
  primeira pessoa (viewmodel) não existem prontas de graça com qualidade: normalmente são
  feitas à mão.
- **Sons:** convertem fácil (WAV 44,1 kHz 16 bits). Freesound tem tiros, passos e ambiente
  bons com CC0/CC-BY.

## Recomendação

1. Agora: texturas CC0 (Poly Haven, ambientCG) para os mapas e sons CC0/CC-BY (Freesound).
2. Props simples convertidos. Planilha de créditos desde o primeiro asset
   (`game/mod_trok/CREDITOS.txt`), com nome, autor, link e licença.
3. Armas e personagens: protótipo com o CS:S montado; os definitivos pedem artista ou
   compra.
