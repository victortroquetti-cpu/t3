# Trok Model

`/model <id>` cria o modelo flutuando um pouco à frente do seu personagem, girando devagar para você ver de todos os lados. Vale para objetos do `gta3.img`, do `gta_int.img` e do `samp.img`, e também para skins e veículos. É só na sua tela: nada vai para o servidor.

**Estado:** v1.0, testado fora do jogo com um MoonLoader de mentira (`test/mock_run.lua`, 36 verificações). Falta o teste no jogo de verdade.

## Instalar

Copie `Trok Model.lua` para a pasta `moonloader\`. Precisa do MoonLoader e do SAMPFUNCS, como o Trok Kill List.

## Comandos

| Comando | O que faz |
|---|---|
| `/model 1337` | cria o modelo 1337 na sua frente; o chat diz o tipo e de qual `.img` ele veio (`1337: objeto do gta3.img`) |
| `/model 19475` | um objeto do SA-MP (`samp.img`) |
| `/model 411` | um veículo, parado no ar, sem colisão e trancado |
| `/model 46` | uma skin, parada no ar e sem colisão |
| `/model` | apaga o modelo |

Um modelo por vez: o novo substitui o anterior. Fechar o script (ou o jogo) também apaga.

## Como ele fica

- **Lugar:** à frente de para onde o personagem está virado. A distância depende do tamanho do modelo, para ele aparecer inteiro, nunca a menos de 2 m. A parte de baixo fica ~0,4 m acima do seu pé.
- **Movimento:** gira 36° por segundo (uma volta a cada 10 s) e fica de frente para você quando nasce. Objetos também sobem e descem 8 cm, flutuando.
- **Sem colisão e parado:** você atravessa o modelo, e ele não cai nem é empurrado.

## O que ele recusa

- **ID fora de 0 a 19999**, ou um número que não é modelo neste jogo. O script confere na tabela de modelos do GTA, a mesma em que o SA-MP registra os do `samp.img`, e também acha a tabela quando um limit adjuster a mudou de lugar.
- **0 (o CJ):** a roupa dele é montada pelo jogo, e ele não nasce como as outras skins.
- **Trens e bonde** (449, 537, 538, 569, 570, 590): o jogo trava se eles forem criados assim.
- **Modelo que não carrega em 3 s:** avisa no chat e não cria nada.

Objetos de horário, como placas de neon e janelas acesas, só aparecem na hora deles, como no jogo.

## Teste sem o jogo

```
cd trok-model && luajit test/mock_run.lua
```

O teste roda o script quadro a quadro num MoonLoader de mentira, com o mundo, a memória do GTA, o chat e o carregamento de modelos. Ele confere:
- o uso, o id inválido, o modelo que não existe e o que não carrega;
- a posição à frente, a altura, a distância pelo tamanho do modelo e o giro;
- o `.img` de cada modelo (`gta3`, `gta_int`, `samp`);
- veículo e skin;
- o trem e o CJ recusados;
- apagar, e o objeto que o jogo apagou sozinho;
- o limit adjuster;
- o fim do script apagando o modelo;
- o arquivo em ASCII puro.

Os nomes das funções do MoonLoader foram conferidos na biblioteca de comandos do GTA SA do Sanny Builder (`sannybuilder/library`), de onde eles vêm, com os mesmos argumentos na mesma ordem.
