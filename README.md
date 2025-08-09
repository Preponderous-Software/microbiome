# microbiome
This application allows the user to witness the activity of a virtual microbial community.

## Example Console Output
```
==============================
    .        o     o  o       
          .        +     .    
 +  o           +  +        . 
       o        +  +  o  o    
    +           o  .  +  o  . 
    o  !  +  !  o             
 +  +        +              o 
          +     +  .  +  +    
 o  +        .  !     !       
       !  o  .     .        o 
==============================

Name: Simulation 1
Size: 10x10
Microorganisms: 45
Dead Microorganisms: 55
Total Energy: 1858

Ticks elapsed: 16 of 120
```

## Testing
### How to run the tests in the dev container
1. Install Docker
2. Clone the repository
3. Open the project in VSCode.
4. Install the Remote-Containers extension.
5. Click on the green button in the bottom left corner of the window.
6. Select "Reopen in Container".
7. Open a terminal in VSCode.
8. Run the `./run_tests.sh` script or build the tests with `make tests` and run them with `./mb_tests`.

## Running the application
### How to run the application with Docker-Compose
1. Install Docker and Docker-Compose
2. Clone the repository
3. Open a terminal in the root directory of the repository
4. Run the following command: `docker-compose up --build --remove-orphans`

### How to run the application in the dev container
1. Install Docker
2. Clone the repository
3. Open the project in VSCode.
4. Install the Remote-Containers extension.
5. Click on the green button in the bottom left corner of the window.
6. Select "Reopen in Container".
7. Open a terminal in VSCode.
8. Run the `./cr.sh` script or build the project with `make` and run it with `./mb_app`.

## Notable Classes

### Microbiome
The Microbiome class represents a virtual microbial community. It is an extension of the Environment class provided by env-lib-cpp. Within the microbiome, there are a number of microbes that are able to interact with each other and the environment. The Microbiome class is responsible for managing the microbes and the environment they exist in.

### Microorganism
The Microorganism class represents a single microbe. It is an extension of the Entity class provided by env-lib-cpp. The Microorganism class is responsible for managing the microbe's energy and metabolism.

## 📄 License

This project is licensed under the **Preponderous Non-Commercial License (Preponderous-NC)**.  
It is free to use, modify, and self-host for **non-commercial** purposes, but **commercial use requires a separate license**.

> **Disclaimer:** *Preponderous Software is not a legal entity.*  
> All rights to works published under this license are reserved by the copyright holder, **Daniel McCoy Stephenson**.

Full license text:  
[https://github.com/Preponderous-Software/preponderous-nc-license/blob/main/LICENSE.md](https://github.com/Preponderous-Software/preponderous-nc-license/blob/main/LICENSE.md)
