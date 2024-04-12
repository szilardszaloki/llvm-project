# Development setup on `macOS`

1. deps  
   - install `CMake` (https://github.com/Kitware/CMake/releases/latest — currently `cmake-3.29.2-macos-universal.dmg`)
   - download `Ninja` (https://github.com/ninja-build/ninja/releases/latest — currently `ninja-mac.zip`) and unzip to `~/Downloads`

From `Terminal`:

2. export deps for the current session
   ```
   export PATH="$PATH:/Applications/CMake.app/Contents/bin"
   ```

   ```
   export PATH="$PATH:/Users/`whoami`/Downloads"
   ```
3. clone the repo
   ```
   mkdir -p ~/git/llvm-project && cd "$_"
   ```

   ```
   git clone https://github.com/szilardszaloki/llvm-project.git .
   ```
4. build `clang` & `clang-tools-extra`
   ```
   mkdir build && cd "$_"
   ```

   ```
   cmake -G Ninja ../llvm -DCMAKE_INSTALL_PREFIX=/usr/local/llvm-project -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DCMAKE_BUILD_TYPE=Release -DLLVM_BUILD_TESTS=ON
   ```

   ```
   ninja
   ```
5. test LLVM
   ```
   ninja check
   ```
6. test Clang
   ```
   ninja clang-test
   ```
7. install `clang` & `clang-tools-extra` to `/usr/local/llvm-project`
   ```
   sudo ninja install
   ```
8. set the Clang just built & installed as its own compiler  
   ```
   cmake -G Ninja ../llvm -DCMAKE_CXX_COMPILER="/usr/local/llvm-project/bin/clang++" -DCMAKE_INSTALL_PREFIX=/usr/local/llvm-project -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DCMAKE_BUILD_TYPE=Release -DLLVM_BUILD_TESTS=ON
   ```

   ```
   ninja
   ```
9. run `state-machine`
   ```
   bin/state-machine
   ```
10. run `state-machine-visualizer`
    ```
    bin/state-machine-visualizer ../clang-tools-extra/state-machine/StateMachine.cpp
    ```

Visual Studio Code:
   - Extensions:
      - [Markdown Preview Mermaid Support](https://marketplace.visualstudio.com/items?itemName=bierner.markdown-mermaid)
      - [Run on Save](https://marketplace.visualstudio.com/items?itemName=emeraldwalk.RunOnSave)
   - `settings.json`:
      ```
      {
        "emeraldwalk.runonsave": {
          "commands": [
            {
              "match": "StateMachine.cpp",
              "cmd": "cd ~/git/llvm-project/build; bin/state-machine-visualizer ../clang-tools-extra/state-machine/StateMachine.cpp"
            }
          ]
        }
      }
      ```
