# ft_ping

Reimplementation de la commande `ping`, projet de l'ecole **42**.

Le code est ecrit en **C99** (`set(CMAKE_C_STANDARD 99)`), et rien n'a ete
emprunte aux revisions suivantes du langage. Cette contrainte a des
consequences visibles dans le code, elles sont expliquees plus bas.

> Etat : **partie 1 terminee**. Toute la couche de fabrication et de relecture
> des paquets ICMP est en place et testee. La partie reseau (socket brut,
> boucle d'envoi, mesure du RTT, statistiques finales) n'est pas encore ecrite.

---

## Ce qui est implemente

Le projet est decoupe en trois couches, chacune ne connaissant que celle
juste en dessous.

| Couche | Dossier | Role |
| --- | --- | --- |
| Tampon d'octets | `src/binary_stream/` | lire et ecrire des entiers et des chaines dans un buffer, avec gestion de l'endianness |
| Registre de paquets | `src/packet_processors/` | associer un id de paquet a ses callbacks de (de)serialisation |
| Specialisation ICMP | `src/icmp/` | en-tete ICMP, checksum, echo request et echo reply |

### 1. `binary_stream` : le tampon d'octets

Un curseur de lecture/ecriture sur un buffer, avec trois tailles :
`capacity` (octets alloues), `index` (curseur) et `max_capacity` (plafond de
croissance).

- **Un seul curseur** pour lire et pour ecrire. Passer de l'ecriture a la
  lecture demande donc un `reset()` explicite, ce qui est plus facile a suivre
  que deux index qui peuvent se desynchroniser.
- Les ecritures **agrandissent** le buffer a la demande jusqu'a
  `max_capacity` ; les lectures ne l'agrandissent jamais et echouent au dela
  de `capacity`.
- Les accesseurs utilisent des **types de largeur fixe** (`int8_t` a
  `int64_t` et leurs equivalents non signes), pour que le nombre d'octets
  ecrits soit le meme partout. Les noms historiques sont gardes :
  `char` -> 8 bits, `short` -> 16, `int` -> 32, `long` -> 64.
- L'**endianness** est un attribut du flux. Le suffixe `_le` force le petit
  boutisme quel que soit ce reglage ; il n'existe pas de variante `_be`, le
  reglage du flux joue ce role.
- Les **chaines** sont prefixees par leur longueur sur 32 bits, et la
  relecture refuse une longueur superieure aux octets restants. C'est ce
  controle qui empeche un buffer corrompu de provoquer une lecture hors
  bornes.
- Chaque operation renvoie un `t_binary_stream_status` **par valeur**, jamais
  de code d'erreur global.

Petite demo, aller-retour sur un flux gros boutiste :

```c
t_binary_stream *stream = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);

stream->methods.write_unsigned_short(stream, 0x1234);
stream->methods.write_string(stream, "pong");

binary_stream_reset(stream);          /* curseur partage : on rembobine */

uint16_t value;
const char *text;
stream->methods.read_unsigned_short(stream, &value);   /* 0x1234 */
stream->methods.read_string(stream, &text);            /* "pong", a free() */

free((void *)text);
binary_stream_free(stream);
```

Un statut se teste sans comparer a la main :

```c
t_binary_stream_status status = stream->methods.read_int(stream, &value);
if (binary_stream_status_is_failed(status)) {
    fprintf(stderr, "lecture: %s\n", binary_stream_status_message(status));
    /* -> "lecture: read past the end of the buffer" */
}
```

### 2. `packet_processors` : le registre

Un tableau indexe par identifiant de paquet (0 a 254). Chaque entree porte
les callbacks du type : `pre_serializer` / `serializer` / `post_serializer`
a l'emission, `constructor` / `pre_deserializer` / `deserializer` /
`destructor` a la reception, et `handler` pour le traitement.

Le decoupage de l'emission en trois etapes n'est pas decoratif : il permet
d'ecrire l'en-tete, puis la charge utile, puis de **revenir poser le
checksum** une fois tous les octets connus. A la reception, la symetrie
permet de verifier le checksum avant de decoder le corps du paquet.

Ajouter un type de paquet ne demande donc aucune modification du code
appelant, seulement un enregistrement :

```c
register_packet_processor(8,                        /* id = type ICMP  */
    &serialize_header_icmp,                          /* pre            */
    &serializer_echo_request,                        /* corps          */
    &serializer_checksum_icmp,                       /* post: checksum */
    NULL, NULL, NULL, NULL, NULL);                   /* rien en lecture */
```

A la reception, l'id est lu dans le premier octet du flux, le paquet est
construit par le constructeur enregistre, et **detruit par le destructeur si
une etape echoue**, pour qu'un paquet malforme ne laisse pas de fuite.

### 3. `icmp` : la specialisation

- `binary_stream_icmp` : ecriture et lecture de l'en-tete (type, code,
  checksum) et calcul du checksum en complement a un sur 16 bits. Le champ
  checksum est remis a zero avant le calcul, comme l'exige la RFC 792.
- `packet/echo_request_packet` : echo request (type 8), identifiant et
  numero de sequence.
- `packet/echo_reply_packet` : echo reply (type 0), fabrique et relecture.
- `packet_processors_icmp` : `init_packet_processors_icmp()` enregistre les
  deux types d'un coup.

### `types.h` et la contrainte C99

En C99, repeter un `typedef` dans deux en-tetes rend illegal tout fichier qui
inclut les deux ; la tolerance n'arrive qu'avec C11. Les alias partages sont
donc declares **une seule fois** dans `src/types.h`, et les en-tetes incluent
ce fichier au lieu de recopier la ligne. Seuls les alias y figurent, jamais le
corps des structures : une declaration avancee suffit pour un pointeur ou un
parametre de fonction.

---

## Demo complete

`main.c` est un programme de demonstration temporaire. Il serialise un echo
request, copie le flux, le transforme en echo reply, recalcule le checksum,
puis le relit comme le ferait la reception :

```c
init_packet_processors_icmp();

t_echo_request *packet = echo_request_new(5, 6);            /* id=5, seq=6 */
t_binary_stream *stream =
    create_binary_stream_with_capacity(sizeof(t_echo_request), BINARY_STREAM_ENDIAN_BIG);

packet_processor_serialize(8, stream, packet);              /* type 8 */
stream->methods.print(stream);

t_binary_stream *copy = stream->methods.copy(stream);
copy->data[0] = 0;                                          /* devient un reply */
binary_stream_icmp_write_checksum(copy, 2, copy->capacity); /* checksum refait */
copy->methods.print(copy);

t_echo_reply *reply = packet_processor_deserializer(copy);  /* id lu dans data[0] */
```

Sortie reelle :

```
t_binary_stream{endian=1,data={08,00,F2,F9,05,06},capacity=6,max_capacity=2147483647,index=6}
t_binary_stream{endian=1,data={00,00,FA,F9,05,06},capacity=6,max_capacity=2147483647,index=0}
check checksum: 0
t_echo_reply{id=5, seq=6}
```

Lecture des octets : `08 00` est l'en-tete (type 8, code 0), `F2 F9` le
checksum calcule apres coup par le `post_serializer`, `05 06` l'identifiant et
le numero de sequence. Sur la copie, le type passe a `00` et le checksum
devient `FA F9` ; `check checksum: 0` confirme que la somme de controle du
paquet complet vaut bien zero, donc que le paquet est intact.

---

## Compilation

```sh
cmake -S . -B build
cmake --build build --target ft_ping
./build/ft_ping
```

## Tests

Deux suites, compilees a part (`EXCLUDE_FROM_ALL`) avec `-Wall -Wextra`.
Chaque cas tourne dans un **processus fils**, de sorte qu'un segfault ou un
abort est rapporte comme un echec au lieu d'emporter toute la suite, ce qui
compte ici puisque les tests provoquent volontairement des lectures hors
bornes et des tampons corrompus.

```sh
cmake --build build --target tests
./build/test_binary_stream        # 41 cas
./build/test_packet_processors    # 46 cas
```

Variante instrumentee, a preferer pendant le developpement :

```sh
cmake --build build --target tests_san    # -fsanitize=address,undefined
./build/test_binary_stream_san
```

Et sous valgrind, si l'outil est installe :

```sh
cmake --build build --target valgrind     # memcheck sur les 3 binaires
```

Les cibles `_san` et les cibles valgrind sont distinctes a dessein : ASan et
valgrind ne peuvent pas instrumenter le meme processus, donc memcheck tourne
sur les binaires nus.

---

## Reste a faire (partie 2)

- ouverture de la socket brute et gestion des privileges
- resolution du nom d'hote passe en argument
- boucle d'envoi, `SIGINT`, `SIGALRM`
- mesure du RTT et statistiques finales (min / avg / max / stddev, perte)
- options de la ligne de commande (`-v`, `-h`, ...)
- remplacement de `main.c` par la vraie boucle de ping
