# Gateway Modbus DNP3 Organizado

Compilação igual ao projeto original:

```bash
cd ~/Projects/RPI_slavednp3
mkdir -p build
cd build
cmake -G Ninja ..
ninja
```

Para executar:

```bash
./slavednp3
```

Estrutura:

- src/Main.cpp: chama e inicia tudo
- src/Modbus.cpp: leitura e escrita Modbus TCP
- src/Dnp3.cpp: outstation DNP3 e comandos vindos do supervisório
- src/Interface.cpp: interface web apenas para leitura/cadastro de pontos
- src/Config.cpp: CSV points_config.csv
- src/Log.cpp: logs
- include/Tipos.h: tipos e protótipos compartilhados
