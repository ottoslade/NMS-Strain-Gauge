# Fuzzing

[What is Fuzzing?](https://en.wikipedia.org/wiki/Fuzzing)

Fuzzing is when you automatically execute generated inputs on your target program and track it's behavior to see if it ever crashes or hangs. Once it does, you can debug the problem.

To fuzz a binary meant for an embedded system, you have to do some kind of emulation or mocking because of how dependent low level code is on the hardware. I think we should use afl-unicorn to emulate because we're already doing a lot of mocking in the sil tests but there are other solutions as well. AFL Unicorn also gives you more control over what parts of the code to fuzz, down to the exact memory address (function) to start at.

### Afl-Unicorn

- [Blog 1 by the creator](https://medium.com/hackernoon/afl-unicorn-fuzzing-arbitrary-binary-code-563ca28936bf)
- [Blog 2 by the creator](https://medium.com/hackernoon/afl-unicorn-part-2-fuzzing-the-unfuzzable-bea8de3540a5)
- [Other blog](https://medium.com/@cy1337/a-basic-guide-to-fuzzing-with-afl-unicorn-mode-f0619ce0bff4)
- [AFL Docs](https://github.com/AFLplusplus/AFLplusplus/blob/stable/unicorn_mode/README.md)
- [LibAFL Example Fuzzer](https://github.com/AFLplusplus/LibAFL/tree/main/fuzzers/full_system/unicorn)
    - LibAFL is AFL++ rewritten in rust by the original AFL++ people