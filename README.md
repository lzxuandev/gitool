# gitool

A lightweight, API-driven cli tool to manage remote Git repository files and directory seamlessly.

<img width="1238" height="668" alt="image" src="https://github.com/user-attachments/assets/1728c11e-b0ff-4393-878b-36171ce57d6d" />


## 🌟 Inspiration (Why gitool ?)

In daily development, we often just want to perform simple operations—like **uploading, downloading, listing, or deleting files**—on a remote Git repository. However, traditional Git workflows impose a heavy tax on your local environment:

- **Directory Pollution**: It forces you to initialize a repository locally, leaving behind bulky and unwanted `.git` hidden folders.
- **Global Environment Pollution**: It requires configuring global files like `~/.gitconfig`, setting up global SSH Keys, and managing complex credential helpers.

`gitool` is born out of the **KISS (Keep It Simple, Stupid)** philosophy and a passion for **zero-pollution**. It communicates directly with upstream platforms using Personal Access Tokens via standard APIs. **No local Git repository environment is required.** It works just like `scp` or `rsync`—intuitive, stateless, and keeping your local project directories pristine.

---

## ✨ Features

- 📦 **Zero Directory Pollution**: No `git init`, no cluttering hidden files left on your local machine.
- ⚡ **Lightweight & Blazing Fast**: Built with pure C, natively compiled with an extremely small footprint.
- 📂 **scp-like Syntax**: Uses the clean and universally understood `username@repo:path` remote destination format.
- 📖 **Native System Integration**: Fully supports standard Linux `man` pages and short aliases (`gt`).

---

## 🛠️ Installation

### Requirement
Ensure the following development libraries are installed on your system:
- `libcurl` 
- `cJSON` 
- `openssl` 

### Building from Source

```bash
# 1. Clone and enter the directory
git clone https://github.com/lzxuandev/gitool.git
cd gitool

# 2. Compile the project
make

# 3. Install globally (Installs binary, man pages, and the 'gt' shortcut)
sudo make install

# 4. Run the command
./build/gitool
#or
gitool
