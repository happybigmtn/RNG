#!/usr/bin/env bash
# Botcoin Universal Installer
# Works on: Linux (x86_64, arm64), macOS (Intel, Apple Silicon), Windows (WSL)
# Usage: curl -fsSL https://raw.githubusercontent.com/happybigmtn/botcoin/main/install.sh | bash

set -e

VERSION="${BOTCOIN_VERSION:-v1.0.1}"
INSTALL_DIR="${BOTCOIN_INSTALL_DIR:-$HOME/.local/bin}"
DATA_DIR="${BOTCOIN_DATA_DIR:-$HOME/.botcoin}"
REPO="happybigmtn/botcoin"
GITHUB_URL="https://github.com/$REPO"

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'

info() { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Detect OS and architecture
detect_platform() {
    OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
    ARCH="$(uname -m)"
    
    case "$OS" in
        linux*)
            # Check for WSL
            if grep -qi microsoft /proc/version 2>/dev/null; then
                OS="windows-wsl"
            else
                OS="linux"
            fi
            ;;
        darwin*) OS="macos" ;;
        mingw*|msys*|cygwin*) OS="windows" ;;
        *) error "Unsupported OS: $OS" ;;
    esac
    
    case "$ARCH" in
        x86_64|amd64) ARCH="x86_64" ;;
        aarch64|arm64) ARCH="arm64" ;;
        *) error "Unsupported architecture: $ARCH" ;;
    esac
    
    PLATFORM="${OS}-${ARCH}"
    info "Detected platform: $PLATFORM"
}

# Check if binary release is available for this platform
check_binary_available() {
    case "$PLATFORM" in
        linux-x86_64|linux-arm64|macos-x86_64|macos-arm64)
            return 0 ;;
        windows-wsl-x86_64|windows-wsl-arm64)
            # WSL uses Linux binaries
            PLATFORM="linux-${ARCH}"
            return 0 ;;
        *)
            return 1 ;;
    esac
}

# Download and install pre-built binary
install_binary() {
    info "Downloading Botcoin $VERSION for $PLATFORM..."
    
    TARBALL="botcoin-${VERSION}-${PLATFORM}.tar.gz"
    URL="$GITHUB_URL/releases/download/${VERSION}/${TARBALL}"
    
    TMPDIR=$(mktemp -d)
    cd "$TMPDIR"
    
    if command -v curl &>/dev/null; then
        curl -fsSL -o "$TARBALL" "$URL" || return 1
    elif command -v wget &>/dev/null; then
        wget -q -O "$TARBALL" "$URL" || return 1
    else
        error "Neither curl nor wget found"
    fi
    
    info "Verifying checksums..."
    tar -xzf "$TARBALL"
    cd release
    
    if command -v sha256sum &>/dev/null; then
        sha256sum -c SHA256SUMS || error "Checksum verification failed!"
    elif command -v shasum &>/dev/null; then
        shasum -a 256 -c SHA256SUMS || error "Checksum verification failed!"
    else
        warn "No checksum tool found, skipping verification"
    fi
    
    success "Checksums verified"
    
    # Install to user directory (no sudo needed)
    mkdir -p "$INSTALL_DIR"
    cp botcoind botcoin-cli "$INSTALL_DIR/"
    chmod +x "$INSTALL_DIR/botcoind" "$INSTALL_DIR/botcoin-cli"
    
    cd /
    rm -rf "$TMPDIR"
    
    return 0
}

# Build from source
build_from_source() {
    info "Building from source (this may take 10-15 minutes)..."
    
    # Install dependencies based on OS
    case "$OS" in
        linux|windows-wsl)
            if command -v apt-get &>/dev/null; then
                info "Installing dependencies via apt..."
                sudo apt-get update
                sudo apt-get install -y build-essential cmake git libboost-all-dev libssl-dev libevent-dev libsqlite3-dev
            elif command -v dnf &>/dev/null; then
                info "Installing dependencies via dnf..."
                sudo dnf install -y cmake gcc-c++ git boost-devel openssl-devel libevent-devel sqlite-devel
            elif command -v pacman &>/dev/null; then
                info "Installing dependencies via pacman..."
                sudo pacman -Sy --noconfirm cmake gcc git boost openssl libevent sqlite
            else
                error "Could not find package manager (apt/dnf/pacman)"
            fi
            ;;
        macos)
            if ! command -v brew &>/dev/null; then
                error "Homebrew not found. Install from https://brew.sh"
            fi
            info "Installing dependencies via Homebrew..."
            brew install cmake boost openssl@3 libevent sqlite
            OPENSSL_FLAG="-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)"
            ;;
    esac
    
    TMPDIR=$(mktemp -d)
    cd "$TMPDIR"
    
    info "Cloning repository..."
    git clone --depth 1 "$GITHUB_URL.git" botcoin
    cd botcoin
    
    info "Getting RandomX..."
    git clone --branch v1.2.1 --depth 1 https://github.com/tevador/RandomX.git src/crypto/randomx
    
    info "Building..."
    cmake -B build \
        -DBUILD_TESTING=OFF \
        -DENABLE_IPC=OFF \
        -DWITH_ZMQ=OFF \
        -DENABLE_WALLET=ON \
        ${OPENSSL_FLAG:-}
    
    cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
    
    # Install
    mkdir -p "$INSTALL_DIR"
    cp build/bin/botcoind build/bin/botcoin-cli "$INSTALL_DIR/"
    chmod +x "$INSTALL_DIR/botcoind" "$INSTALL_DIR/botcoin-cli"
    
    cd /
    rm -rf "$TMPDIR"
    
    success "Built and installed from source"
}

# Create default config
setup_config() {
    mkdir -p "$DATA_DIR"
    
    if [ -f "$DATA_DIR/botcoin.conf" ]; then
        warn "Config already exists at $DATA_DIR/botcoin.conf"
        return
    fi
    
    RPCPASS=$(openssl rand -hex 16 2>/dev/null || head -c 32 /dev/urandom | xxd -p | head -c 32)
    
    cat > "$DATA_DIR/botcoin.conf" << EOF
server=1
daemon=1
rpcuser=agent
rpcpassword=$RPCPASS
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
addnode=95.111.227.14:8433
addnode=95.111.229.108:8433
addnode=95.111.239.142:8433
addnode=161.97.83.147:8433
addnode=161.97.97.83:8433
addnode=161.97.114.192:8433
addnode=161.97.117.0:8433
addnode=194.163.144.177:8433
addnode=185.218.126.23:8433
addnode=185.239.209.227:8433
EOF
    
    success "Config created at $DATA_DIR/botcoin.conf"
}

# Add to PATH
setup_path() {
    if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
        warn "$INSTALL_DIR is not in your PATH"
        
        SHELL_RC=""
        if [ -n "$BASH_VERSION" ] && [ -f "$HOME/.bashrc" ]; then
            SHELL_RC="$HOME/.bashrc"
        elif [ -n "$ZSH_VERSION" ] && [ -f "$HOME/.zshrc" ]; then
            SHELL_RC="$HOME/.zshrc"
        elif [ -f "$HOME/.profile" ]; then
            SHELL_RC="$HOME/.profile"
        fi
        
        if [ -n "$SHELL_RC" ]; then
            echo "export PATH=\"\$PATH:$INSTALL_DIR\"" >> "$SHELL_RC"
            success "Added $INSTALL_DIR to PATH in $SHELL_RC"
            info "Run: source $SHELL_RC"
        else
            info "Add to your shell config: export PATH=\"\$PATH:$INSTALL_DIR\""
        fi
    fi
}

# Verify installation
verify_install() {
    export PATH="$PATH:$INSTALL_DIR"
    
    if ! command -v botcoind &>/dev/null; then
        error "botcoind not found after installation"
    fi
    
    info "Verifying installation..."
    "$INSTALL_DIR/botcoind" --version
    success "Botcoin installed successfully!"
}

# Print next steps
print_next_steps() {
    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  Botcoin installed successfully!${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "Next steps:"
    echo ""
    echo "  1. Start the daemon:"
    echo "     $INSTALL_DIR/botcoind -daemon"
    echo ""
    echo "  2. Check status (wait 10s for startup):"
    echo "     $INSTALL_DIR/botcoin-cli getblockchaininfo"
    echo ""
    echo "  3. Create wallet and start mining:"
    echo "     $INSTALL_DIR/botcoin-cli createwallet \"miner\""
    echo "     ADDR=\$($INSTALL_DIR/botcoin-cli -rpcwallet=miner getnewaddress)"
    echo "     $INSTALL_DIR/botcoin-cli -rpcwallet=miner generatetoaddress 1 \"\$ADDR\""
    echo ""
    echo "  4. Stop daemon:"
    echo "     $INSTALL_DIR/botcoin-cli stop"
    echo ""
    echo "Uninstall:"
    echo "  rm -rf $INSTALL_DIR/botcoin* $DATA_DIR"
    echo ""
    echo "Docs: https://github.com/$REPO"
    echo "Skill: https://clawhub.ai/happybigmtn/botcoin-miner"
    echo ""
}

# Main
main() {
    echo ""
    echo "╔══════════════════════════════════════════╗"
    echo "║  Botcoin Installer                       ║"
    echo "║  The cryptocurrency for AI agents        ║"
    echo "╚══════════════════════════════════════════╝"
    echo ""
    
    # Check if already installed (idempotent)
    if command -v botcoind &>/dev/null && [ "$1" != "--force" ]; then
        INSTALLED_VERSION=$(botcoind --version 2>/dev/null | head -1 || echo "unknown")
        success "Botcoin already installed: $INSTALLED_VERSION"
        info "Location: $(which botcoind)"
        info "To reinstall, run: $0 --force"
        info "To uninstall: rm -rf $INSTALL_DIR/botcoin* $DATA_DIR"
        exit 0
    fi
    
    detect_platform
    
    # Try binary first, fall back to source
    if check_binary_available; then
        if install_binary; then
            success "Installed from pre-built binary"
        else
            warn "Binary download failed, building from source..."
            build_from_source
        fi
    else
        warn "No pre-built binary for $PLATFORM, building from source..."
        build_from_source
    fi
    
    setup_config
    setup_path
    verify_install
    print_next_steps
}

main "$@"
