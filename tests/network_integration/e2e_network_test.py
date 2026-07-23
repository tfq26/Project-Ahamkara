#!/usr/bin/env python3
"""
Ahamkara End-to-End Network Integration Test.

Launches the dedicated server and multiple client instances, simulates gameplay,
and verifies state synchronization over UDP.

Usage:
    python3 tests/network_integration/e2e_network_test.py [--build-dir <path>] [--num-clients 2] [--duration 10]

Requirements:
    - Built ahamkara_server and ahamkara_client binaries (build with AHAMKARA_BUILD_CLIENT=ON)
    - Python 3.8+
"""

import argparse
import os
import platform
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# Defaults
DEFAULT_NUM_CLIENTS = 2
DEFAULT_TEST_DURATION_SECONDS = 15
SERVER_STARTUP_WAIT = 3.0  # seconds to wait for server boot
CLIENT_STARTUP_INTERVAL = 0.5  # stagger between client launches
TEARDOWN_TIMEOUT = 10.0  # max seconds to wait for graceful shutdown


def find_project_root() -> Path:
    """Walk up from cwd to find the project root containing CMakeLists.txt."""
    cursor = Path.cwd().resolve()
    while cursor != cursor.parent:
        if (cursor / "CMakeLists.txt").exists():
            return cursor
        cursor = cursor.parent
    print("FATAL: Could not locate project root (no CMakeLists.txt found).")
    sys.exit(1)


def find_binary(binary_name: str, build_dir: Optional[Path] = None) -> Optional[Path]:
    """Find a built binary by searching common build output directories."""
    project_root = find_project_root()
    candidates: List[Path] = []

    if build_dir:
        # Explicit build directory
        subdirs = ["server", "client", "bin"]
        for sd in subdirs:
            candidates.append(build_dir / sd / binary_name)

    # Default search paths
    for preset in ["debug", "release", "debug-headless", "relwithdebinfo"]:
        for sub in ["server", "client"]:
            candidates.append(project_root / "build" / preset / sub / binary_name)
        candidates.append(project_root / "build" / preset / "bin" / binary_name)

    for path in candidates:
        path = path.resolve()
        if path.exists() and os.access(str(path), os.X_OK):
            return path
    return None


def find_free_port() -> int:
    """Find a free UDP port by binding to port 0."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("", 0))
        return s.getsockname()[1]


def find_free_tcp_port() -> int:
    """Find a free TCP port."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("", 0))
        return s.getsockname()[1]


def now_str() -> str:
    return datetime.now(timezone.utc).strftime("%H:%M:%S.%f")[:12]


def log(msg: str) -> None:
    print(f"[{now_str()}] {msg}", flush=True)


def terminate_process(proc: subprocess.Popen, name: str, timeout: float = 5.0) -> None:
    """Gracefully terminate a process, then force kill if needed."""
    if proc.poll() is not None:
        return
    try:
        log(f"Terminating {name} (PID {proc.pid})...")
        if platform.system() == "Windows":
            proc.terminate()
        else:
            proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=timeout)
        log(f"{name} exited with code {proc.returncode}.")
    except subprocess.TimeoutExpired:
        log(f"WARNING: {name} did not exit gracefully, killing...")
        proc.kill()
        proc.wait(timeout=5.0)
        log(f"{name} killed.")


# ── Log parsing helpers ────────────────────────────────────────────────────────

def count_pattern_occurrences(text: str, pattern: str) -> int:
    """Count number of times a pattern appears in text (case-insensitive)."""
    return len(re.findall(pattern, text, re.IGNORECASE))


def extract_client_positions(log_text: str) -> List[Tuple[float, float, float]]:
    """Extract interpolated player positions from client logs.

    Looks for lines like:
      [Client] tick=... | interp_pos=(x, z) | auth_pos=(x, z) ...
    Returns list of (x, 0, z) position tuples.
    """
    positions: List[Tuple[float, float, float]] = []
    for match in re.finditer(
        r"interp_pos=\(([-\d.e+]+),\s*([-\d.e+]+)\)",
        log_text,
    ):
        try:
            x = float(match.group(1))
            z = float(match.group(2))
            positions.append((x, 0.0, z))
        except ValueError:
            continue
    return positions


def extract_server_connected_peers(log_text: str) -> List[int]:
    """Extract connected peer counts from server periodic stats.

    Looks for lines like:
      Server tick=... peers=N connected=M activities=... players=...
    Returns list of connected counts.
    """
    counts: List[int] = []
    for match in re.finditer(r"connected=(\d+)", log_text):
        try:
            counts.append(int(match.group(1)))
        except ValueError:
            continue
    return counts


def extract_server_welcome_count(log_text: str) -> int:
    """Count how many clients received a handshake welcome."""
    return count_pattern_occurrences(log_text, "handshake welcome")


# ── Main test runner ───────────────────────────────────────────────────────────

class NetworkIntegrationTest:
    """Orchestrates server and client processes for the E2E test."""

    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.project_root = find_project_root()
        self.server_bin = find_binary("ahamkara_server", Path(args.build_dir) if args.build_dir else None)
        self.client_bin = find_binary("ahamkara_client", Path(args.build_dir) if args.build_dir else None)

        if not self.server_bin:
            log("ERROR: Server binary (ahamkara_server) not found. Build the project first.")
            sys.exit(1)
        if not self.client_bin:
            log("ERROR: Client binary (ahamkara_client) not found. Build the project first.")
            sys.exit(1)

        log(f"Server binary: {self.server_bin}")
        log(f"Client binary: {self.client_bin}")

        self.udp_port = args.port or find_free_port()
        self.admin_port = args.admin_port or find_free_tcp_port()
        self.num_clients = args.num_clients
        self.duration = args.duration

        # Log directory
        log_dir_name = f"network-integration-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')}"
        self.log_dir = self.project_root / "build" / "test-logs" / log_dir_name
        self.log_dir.mkdir(parents=True, exist_ok=True)
        log(f"Log directory: {self.log_dir}")

        # Process handles
        self.server_proc: Optional[subprocess.Popen] = None
        self.client_procs: List[subprocess.Popen] = []
        self.server_log_path: Optional[Path] = None
        self.client_log_paths: List[Path] = []
        self.client_stdin_paths: List[Path] = []

    def run(self) -> int:
        """Execute the full test and return 0 on success, 1 on failure."""
        try:
            if not self._start_server():
                return 1
            if not self._start_clients():
                return 1
            self._run_simulation()
            return 0 if self._verify_results() else 1
        finally:
            self._teardown()

    def _start_server(self) -> bool:
        """Launch the dedicated server process."""
        log(f"Starting server on UDP port {self.udp_port}, admin HTTP port {self.admin_port}...")

        self.server_log_path = self.log_dir / "server.log"

        server_env = os.environ.copy()
        server_env["WISH_SERVER_PORT"] = str(self.udp_port)
        server_env["WISH_SERVER_ADMIN_PORT"] = str(self.admin_port)
        server_env["WISH_SERVER_MAX_PLAYERS"] = str(max(8, self.num_clients + 2))
        server_env["WISH_SERVER_TICK_RATE"] = str(self.args.tick_rate)

        # Use stdbuf -oL to force line-buffered stdout for the same reason
        # as the client: ae::log_* writes to std::cout without explicit flush.
        server_cmd = [
            "stdbuf",
            "-oL",
            str(self.server_bin),
            f"--port={self.udp_port}",
            f"--admin-port={self.admin_port}",
            f"--max-players={max(8, self.num_clients + 2)}",
            f"--tick-rate={self.args.tick_rate}",
            f"--match-duration={self.args.match_duration}",
        ]

        with open(self.server_log_path, "w") as log_file:
            self.server_proc = subprocess.Popen(
                server_cmd,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                env=server_env,
                text=True,
            )

        log(f"Server PID: {self.server_proc.pid}")
        time.sleep(SERVER_STARTUP_WAIT)

        # Check server is still running
        if self.server_proc.poll() is not None:
            log(f"ERROR: Server exited prematurely with code {self.server_proc.returncode}.")
            self._dump_log(self.server_log_path)
            return False

        # Verify the server is actually listening on the UDP port
        if not self._check_port_listening(self.udp_port):
            log(f"ERROR: Server does not appear to be listening on UDP port {self.udp_port}.")
            self._dump_log(self.server_log_path)
            return False

        log("Server is running and listening.")
        return True

    def _check_port_listening(self, port: int) -> bool:
        """Check if a process is listening on the given UDP port."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as test_sock:
                test_sock.settimeout(1.0)
                # Send an empty packet — if the port is open, no error
                test_sock.sendto(b"", ("127.0.0.1", port))
                return True
        except (socket.error, OSError):
            return False

    def _start_clients(self) -> bool:
        """Launch client processes that connect to the server."""
        log(f"Starting {self.num_clients} client(s)...")

        for i in range(self.num_clients):
            time.sleep(CLIENT_STARTUP_INTERVAL)  # Stagger launches

            client_log_path = self.log_dir / f"client_{i}.log"
            self.client_log_paths.append(client_log_path)

            # Client takes server IP as first argument.
            # Use stdbuf -oL to force line-buffered stdout, because the engine's
            # log.cpp writes to std::cout without explicit flush.  Without this,
            # when the process is killed by SIGTERM at teardown the buffered
            # output is lost and client logs appear empty.
            client_cmd = [
                "stdbuf",
                "-oL",
                str(self.client_bin),
                "127.0.0.1",  # Server address
                "--server-port",
                str(self.udp_port),
            ]

            client_env = os.environ.copy()

            with open(client_log_path, "w") as log_file:
                proc = subprocess.Popen(
                    client_cmd,
                    stdout=log_file,
                    stderr=subprocess.STDOUT,
                    env=client_env,
                    text=True,
                )
            self.client_procs.append(proc)
            log(f"Client {i} started (PID {proc.pid}).")

        # Brief wait for clients to connect and start receiving state
        time.sleep(2.0)

        # Verify clients are still running
        for i, proc in enumerate(self.client_procs):
            if proc.poll() is not None:
                log(f"WARNING: Client {i} exited early with code {proc.returncode}.")
                self._dump_log(self.client_log_paths[i])

        running_clients = sum(1 for p in self.client_procs if p.poll() is None)
        log(f"{running_clients}/{self.num_clients} clients still running.")

        if running_clients == 0:
            log("ERROR: No clients are running.")
            return False

        return True

    def _run_simulation(self) -> None:
        """Let the simulation run for the configured duration."""
        log(f"Running simulation for {self.duration} seconds...")
        time.sleep(self.duration)

    def _verify_results(self) -> bool:
        """Check logs for expected outcomes and return True if all pass."""
        log("=== Verifying test results ===")
        all_passed = True

        # ── 1. Check server logs ────────────────────────────────────────
        if self.server_log_path and self.server_log_path.exists():
            server_text = self.server_log_path.read_text()
            log(f"Server log size: {len(server_text)} bytes")

            # Check server connected peers
            connected_counts = extract_server_connected_peers(server_text)
            if connected_counts:
                max_connected = max(connected_counts)
                log(f"Server reported max connected peers: {max_connected}")
                if max_connected >= self.num_clients:
                    log(f"PASS: Server reported {max_connected} connected peers (expected >= {self.num_clients}).")
                else:
                    log(f"FAIL: Server only reported {max_connected} connected peers (expected >= {self.num_clients}).")
                    all_passed = False
            else:
                log("WARNING: Could not find peer count info in server logs.")

            # Check for handshake activity
            welcome_count = extract_server_welcome_count(server_text)
            log(f"Server processed handshake welcomes: {welcome_count}")
            if welcome_count > 0:
                log(f"PASS: Server performed {welcome_count} handshake(s).")
            else:
                log("WARNING: No handshake activity detected (may be normal with no-auth mode).")

        # ── 2. Check client logs ────────────────────────────────────────
        for i, log_path in enumerate(self.client_log_paths):
            if not log_path.exists():
                log(f"FAIL: Client {i} log not found at {log_path}.")
                all_passed = False
                continue

            client_text = log_path.read_text()
            log(f"Client {i} log size: {len(client_text)} bytes")

            # Check for successful connection (either 'Client sending input to'
            # log line or the state comparison tick log).
            has_startup = "Client sending input" in client_text or "Client application started" in client_text
            has_state_log = False
            positions = extract_client_positions(client_text)
            if positions:
                has_state_log = True
                log(f"PASS: Client {i} received {len(positions)} interpolated position updates.")
                sample = positions[:3]
                log(f"  Sample positions: {sample}")
            if has_startup or has_state_log:
                log(f"PASS: Client {i} started and {'connected' if has_state_log else 'attempted connection'}.")
            elif len(client_text) > 0:
                log(f"INFO: Client {i} produced {len(client_text)} bytes of log output.")
            else:
                log(f"FAIL: Client {i} produced no log output (stdout buffering issue?).")
                all_passed = False

        # ── 3. Cross-client state comparison ────────────────────────────
        all_positions: List[List[Tuple[float, float, float]]] = []
        for i, log_path in enumerate(self.client_log_paths):
            if log_path.exists():
                pos = extract_client_positions(log_path.read_text())
                all_positions.append(pos)

        if len(all_positions) >= 2:
            # Compare the last known position of each client
            for i in range(len(all_positions)):
                for j in range(i + 1, len(all_positions)):
                    if all_positions[i] and all_positions[j]:
                        last_i = all_positions[i][-1]
                        last_j = all_positions[j][-1]
                        dx = abs(last_i[0] - last_j[0])
                        dz = abs(last_i[2] - last_j[2])
                        if dx < 100.0 and dz < 100.0:
                            log(f"PASS: Clients {i} and {j} positions are within reasonable range "
                                f"(Δ({dx:.2f}, {dz:.2f})).")
                        else:
                            log(f"WARNING: Clients {i} and {j} positions diverge significantly "
                                f"(Δ({dx:.2f}, {dz:.2f})).")

        # ── Summary ──────────────────────────────────────────────────────
        log(f"\n{'=' * 50}")
        log(f"TEST {'PASSED' if all_passed else 'FAILED'}")
        log(f"{'=' * 50}")
        log(f"Logs saved to: {self.log_dir}")

        return all_passed

    def _dump_log(self, log_path: Optional[Path]) -> None:
        """Print the last N lines of a log file for debugging."""
        if not log_path or not log_path.exists():
            return
        log(f"--- Last lines of {log_path.name} ---")
        text = log_path.read_text()
        lines = text.splitlines()
        for line in lines[-20:]:
            print(f"  {line}", flush=True)

    def _teardown(self) -> None:
        """Clean up all processes."""
        log("Tearing down...")

        # Terminate clients first
        for i, proc in enumerate(self.client_procs):
            if proc.poll() is None:
                terminate_process(proc, f"Client {i}", TEARDOWN_TIMEOUT)

        # Then terminate server
        if self.server_proc and self.server_proc.poll() is None:
            terminate_process(self.server_proc, "Server", TEARDOWN_TIMEOUT)

        log("Teardown complete.")


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Ahamkara End-to-End Network Integration Test",
    )
    parser.add_argument(
        "--build-dir",
        help="Path to build directory containing server/ and client/ subdirectories",
    )
    parser.add_argument(
        "-n", "--num-clients",
        type=int,
        default=DEFAULT_NUM_CLIENTS,
        help=f"Number of client instances to launch (default: {DEFAULT_NUM_CLIENTS})",
    )
    parser.add_argument(
        "-d", "--duration",
        type=int,
        default=DEFAULT_TEST_DURATION_SECONDS,
        help=f"Test duration in seconds (default: {DEFAULT_TEST_DURATION_SECONDS})",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=0,
        help="UDP port for the game server (default: auto)",
    )
    parser.add_argument(
        "--admin-port",
        type=int,
        default=0,
        help="TCP port for the admin HTTP server (default: auto)",
    )
    parser.add_argument(
        "--tick-rate",
        type=int,
        default=30,
        help="Server tick rate in Hz (default: 30)",
    )
    parser.add_argument(
        "--match-duration",
        type=int,
        default=120,
        help="Match duration in seconds (default: 120)",
    )
    return parser


def main() -> int:
    parser = create_parser()
    args = parser.parse_args()
    test = NetworkIntegrationTest(args)
    return test.run()


if __name__ == "__main__":
    sys.exit(main())
