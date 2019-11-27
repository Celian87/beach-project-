# Beach Project

This project was designed to pass the practical examination of Operation System so it had guidelines and features to be implemented obligatorily. These are described in [Spiaggia](./Spiaggia.pdf)

The aim of this project is to create a client server system for managing the occupations of beach umbrellas, able to provide information on the availability of umbrellas, and commit or cancel them.
The architecture is client-server type and the student is asked to create the server and the client. The main objective of the project is to familiarize the student with sockets and concurrent programming, as well as with client-server architectures.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Prerequisites

**For Ubuntu:**

- GCC Compiler

### Installing

After `git clone`, go to the terminal in the **repository directory** and run these command:
- `sh build.sh` to generate server.o and client.o

## Running

- Open a second new terminal in the same directory
- In the first terminal run `./server`
- In the second terminal run `./client` -> In this case there will be options to choose from. Follow the instructions.
    For example -> run `./client BOOK` to make a reservation
                -> run `./client AVAILABLE` to know how many beach umbrellas are available

## Built With
- C 

## Authors

* **Giulia Vantaggiato** [Giulia Vantaggiato](https://github.com/Celian87)
* **Dalila Damiano**

## License

This project is licensed under the MIT License - see the [LICENSE.md](./LICENSE) file for details
