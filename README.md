# GUI Application to Chat with Local LLMs

A user-friendly GUI-based application that enables interaction with locally hosted Large Language Models (LLMs) using socket programming and GTK for the interface.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Project Overview

This project implements a client-server architecture where:
- The **client** provides a GTK-based GUI for user interactions
- The **server** hosts the LLM and processes queries via socket communication
- Communication happens through socket programming for low-latency responses

## Features

- Clean and responsive GTK-based GUI for chat interactions
- Local LLM model integration (with support for LLaMA, Mistral, GPT-J, and custom models)
- Socket-based architecture for modular communication
- Message history with timestamps
- Lightweight and offline-friendly setup

## Prerequisites

- GCC compiler
- GTK3 development libraries
- pthread support
- Local LLM setup (e.g., Ollama)

### Installing Dependencies

#### On macOS:

```bash
brew install gtk+3
```

#### On Ubuntu/Debian:

```bash
sudo apt-get install libgtk-3-dev
```

## Building the Project

Use the provided Makefile to build the project:

```bash
make
```

This will create two executables in the `bin` directory:
- `llm_server`: The server component that interfaces with the LLM
- `llm_client`: The GTK client for user interaction

## Running the Application

### Start the Server

```bash
make run-server
# or directly
./bin/llm_server
```

Server options:
- `--port PORT`: Server port (default: 8080)
- `--model TYPE`: Model type (llama, mistral, gptj, custom)
- `--model-path PATH`: Path to model file
- `--temperature VALUE`: Temperature for generation (default: 0.7)
- `--max-tokens VALUE`: Maximum tokens to generate (default: 512)

### Start the Client

```bash
make run-client
# or directly
./bin/llm_client
```

Client options:
- `--server ADDRESS`: Server address (default: 127.0.0.1)
- `--port PORT`: Server port (default: 8080)

## Getting Started

### Cloning the Repository

```bash
# Clone the repository
git clone https://github.com/USERNAME/local-llm-gui.git
cd local-llm-gui

# Build the project
make
```

## Project Structure

```
.
├── bin/                  # Compiled binaries
├── build/                # Build artifacts
├── src/                  # Source code
│   ├── client/           # Client application
│   │   ├── client.c      # Client main program
│   │   ├── gui.c         # GTK GUI implementation
│   │   └── gui.h         # GUI header
│   ├── common/           # Shared components
│   │   ├── socket_utils.c # Socket utilities
│   │   └── socket_utils.h # Socket header
│   └── server/           # Server application
│       ├── llm_interface.c # LLM integration
│       ├── llm_interface.h # LLM header
│       ├── server.c      # Server main program
│       └── server.h      # Server header
├── .gitignore           # Git ignore file
└── Makefile              # Build configuration
```

## Team Members

- Ismoil Salohiddinov (ID: 220626)
- Komiljon Qosimov (ID: 220493)

## Future Enhancements

- Multi-turn chat memory and session persistence
- Voice input and speech-to-text integration
- Custom model fine-tuning and plugin extensions

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'feat: add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.
