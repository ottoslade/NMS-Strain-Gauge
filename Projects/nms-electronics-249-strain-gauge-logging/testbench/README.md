# Nittany Motorsports Testing and Validation

Welcome to the Nittany Motorsports Testing and Validation repository! This repository contains all the information, documentation, and resources related to the testing and validation processes for our team.

## Table of Contents

- [Introduction](#introduction)
- [Folder Structure](#folder-structure)
- [Setup and Installation](#setup-and-installation)
- [Contributing](#contributing)
- [License](#license)

## Introduction

This repository serves as the central hub for managing our testing and validation efforts. We have three main testing approaches:
1) Use a Raspberry Pi to simulate our car
2) Use the fake function framework to create mock tests
3) Use afl-unicorn to emulate FreeRTOS and test our code's functionality

## Folder Structure

- `/hil`: Hardware in the loop testbench
- `/nms_fuzz`: Embedded fuzzing with afl-unicorn
- `/sil`: Software in the loop testbench

## Contributing

Please follow these guidelines:

1. Create a new issue and branch for your work.
2. Make your changes and document them thoroughly.
3. Test your changes to ensure they work as expected.
4. Submit a pull request with a clear description of your changes.
5. Link any issue ticket related to the changes made.

## License

If we ever establish licensing then list those here.

---

**Note:** This README was written by ChatGPT, thanks ChatGPT.
