# minmea (componente local)

Parser NMEA 0183 de Kosma Moczek, copiado tal cual desde
https://github.com/kosma/minmea (commit 2dd2cd11a359de5583e68053182d5bbf29725934).

No existe en el registro de componentes de Espressif, por eso va como
componente local en vez de como dependencia gestionada. Para actualizarlo
basta con sustituir `minmea.c` e `include/minmea.h` por los del upstream.

Licencia: WTFPL (ver `COPYING`).
