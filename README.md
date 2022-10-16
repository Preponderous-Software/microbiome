# microbiome
This application allows the user to witness the activity of a virtual microbial community.

## How to run the application with Docker-Compose
1. Install Docker and Docker-Compose
2. Clone the repository
3. Open a terminal in the root directory of the repository
4. Run the following command: `docker-compose up --build --remove-orphans`

## How to run the application in the dev container
1. Install Docker
2. Clone the repository
3. Open the project in VSCode.
4. Install the Remote-Containers extension.
5. Click on the green button in the bottom left corner of the window.
6. Select "Reopen in Container".
7. Open a terminal in VSCode.
8. Run the `./cr.sh` script or build the project with `make` and run it with `./mb_app'.

## Classes
### MicrobiomeApplication
This class is the main class of the application. It is responsible for the main loop.

### Microbiome
The Microbiome class represents a virtual microbial community. It is an extension of the Environment class provided by env-lib-cpp. Within the microbiome, there are a number of microbes that are able to interact with each other and the environment. The Microbiome class is responsible for managing the microbes and the environment.

### Microorganism
The Microorganism class represents a single microbe. It is an extension of the Entity class provided by env-lib-cpp. The Microorganism class is responsible for managing the microbe's behavior and its interactions with the environment and other microbes.

### Config
The Config class contains a number of constants that are used throughout the application. These constants can be changed to alter the behavior of the application.

## Example Console Output
```
microbiome_1  | ==============================
microbiome_1  |           O           O     O
microbiome_1  |  O           O
microbiome_1  |                    O     O
microbiome_1  |        O        O           O
microbiome_1  |                             O
microbiome_1  |  O     O           O  O     O
microbiome_1  |     O  O     O           O  O
microbiome_1  |  O        O
microbiome_1  |     O
microbiome_1  |              O  O
microbiome_1  | ==============================
microbiome_1  |
microbiome_1  | Name: microbiome
microbiome_1  | Size: 10x10
microbiome_1  | Microorganisms: 30
microbiome_1  |
microbiome_1  | Ticks Left: 88
```
