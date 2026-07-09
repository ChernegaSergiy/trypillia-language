#!/usr/bin/env bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Trypillia Programming Language Installer ===${NC}"

# 1. Check dependencies
echo -e "${BLUE}[1/5] Checking dependencies...${NC}"
MISSING_DEPS=0
for dep in git cmake; do
    if ! command -v $dep &> /dev/null; then
        echo -e "${RED}Error: '$dep' is not installed. Please install it first.${NC}"
        MISSING_DEPS=1
    fi
done

if ! command -v cc &> /dev/null && ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
    echo -e "${RED}Error: No C/C++ compiler found. Please install gcc or clang.${NC}"
    MISSING_DEPS=1
fi

if [ $MISSING_DEPS -eq 1 ]; then
    exit 1
fi
echo -e "${GREEN}Dependencies OK.${NC}"

# 2. Clone repository
SRC_DIR="$HOME/.trypillia-src"
echo -e "${BLUE}[2/5] Cloning repository to $SRC_DIR...${NC}"
if [ -d "$SRC_DIR" ]; then
    echo "Directory already exists. Updating..."
    cd "$SRC_DIR"
    git fetch --all
    git reset --hard origin/main
else
    git clone https://github.com/ChernegaSergiy/trypillia-language.git "$SRC_DIR"
    cd "$SRC_DIR"
fi

# 3. Build using cross-platform cmake commands
echo -e "${BLUE}[3/5] Building Trypillia (this might take a few minutes)...${NC}"
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4

# 4. Install
echo -e "${BLUE}[4/5] Installing binary...${NC}"
INSTALL_DIR="$HOME/.local/bin"
mkdir -p "$INSTALL_DIR"
cp build/trypillia "$INSTALL_DIR/"
cp build/trypillia-lsp "$INSTALL_DIR/" 2>/dev/null || true
echo -e "${GREEN}Installed to $INSTALL_DIR/trypillia${NC}"

# 5. Path Setup
echo -e "${BLUE}[5/5] Checking PATH...${NC}"
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo -e "${RED}Warning: $INSTALL_DIR is not in your PATH.${NC}"
    echo "Add this line to your ~/.bashrc, ~/.zshrc, or ~/.profile to use 'trypillia' from anywhere:"
    echo -e "${GREEN}export PATH=\"$INSTALL_DIR:\$PATH\"${NC}"
else
    echo -e "${GREEN}PATH is already configured!${NC}"
fi

echo -e "${GREEN}=== Installation Complete! ===${NC}"
echo -e "Run ${BLUE}trypillia${NC} to start."
