#!/usr/bin/env python3
"""
Local web UI for playing against the engine.

Zero external dependencies beyond python-chess (`pip install chess`):
serves a small board page and drives the compiled `engine` binary as a
UCI subprocess. python-chess handles move legality, SAN, and game-over
detection for the human side; the engine binary is only ever asked for
its own move, exactly the way a real UCI GUI would use it.

Usage:
    python3 ui/server.py [port]

Then open http://127.0.0.1:<port> (default 8765).
"""

import http.server
import json
import os
import socketserver
import subprocess
import sys
import threading
import urllib.parse

import chess

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE_PATH = os.path.join(PROJECT_ROOT, "engine")
STATIC_DIR = os.path.abspath(os.path.dirname(__file__))


def ensure_engine_built():
    if not os.path.isfile(ENGINE_PATH):
        print("engine binary not found, building via 'make'...", file=sys.stderr)
        subprocess.run(["make"], cwd=PROJECT_ROOT, check=True)


class Engine:
    """A running UCI engine subprocess."""

    def __init__(self, path):
        self.proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._send("uci")
        self._read_until("uciok")
        self._send("isready")
        self._read_until("readyok")

    def _send(self, line):
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()

    def _read_until(self, token):
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError("engine process ended unexpectedly")
            if line.strip().startswith(token):
                return

    def new_game(self):
        self._send("ucinewgame")

    def best_move(self, uci_moves, movetime_ms):
        moves_part = ("moves " + " ".join(uci_moves)) if uci_moves else ""
        self._send(("position startpos " + moves_part).rstrip())
        self._send(f"go movetime {movetime_ms}")
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError("engine process ended unexpectedly")
            line = line.strip()
            if line.startswith("bestmove"):
                return line.split()[1]

    def quit(self):
        try:
            self._send("quit")
            self.proc.wait(timeout=2)
        except Exception:
            self.proc.kill()


class GameState:
    def __init__(self, engine):
        self.engine = engine
        self.lock = threading.Lock()
        self.board = chess.Board()
        self.human_color = chess.WHITE
        self.uci_moves = []
        self.san_history = []
        self.movetime_ms = 1000

    def new_game(self, human_color_str, movetime_ms):
        with self.lock:
            self.board = chess.Board()
            self.human_color = chess.WHITE if human_color_str == "white" else chess.BLACK
            self.uci_moves = []
            self.san_history = []
            self.movetime_ms = movetime_ms
            self.engine.new_game()
            if self.board.turn != self.human_color and not self.board.is_game_over():
                self._engine_move_locked()
            return self._status_locked()

    def _engine_move_locked(self):
        best = self.engine.best_move(self.uci_moves, self.movetime_ms)
        if best in (None, "0000"):
            return
        move = chess.Move.from_uci(best)
        self.san_history.append(self.board.san(move))
        self.board.push(move)
        self.uci_moves.append(best)

    def human_move(self, uci_str):
        with self.lock:
            if self.board.turn != self.human_color:
                raise ValueError("not your turn")
            try:
                move = chess.Move.from_uci(uci_str)
            except ValueError:
                raise ValueError("malformed move")
            if move not in self.board.legal_moves:
                raise ValueError("illegal move")

            self.san_history.append(self.board.san(move))
            self.board.push(move)
            self.uci_moves.append(uci_str)

            if not self.board.is_game_over():
                self._engine_move_locked()

            return self._status_locked()

    def undo(self):
        with self.lock:
            # pop back to the human's turn: one ply if the engine hasn't
            # replied yet (or there's only one ply played), else two
            n = 2 if len(self.uci_moves) >= 2 else len(self.uci_moves)
            for _ in range(n):
                self.board.pop()
            if n:
                self.uci_moves = self.uci_moves[:-n]
                self.san_history = self.san_history[:-n]
            return self._status_locked()

    def legal_destinations(self, square_name):
        with self.lock:
            try:
                sq = chess.parse_square(square_name)
            except ValueError:
                return []
            dests = []
            for m in self.board.legal_moves:
                if m.from_square == sq:
                    dests.append({
                        "to": chess.square_name(m.to_square),
                        "promotion": m.promotion is not None,
                    })
            return dests

    def status(self):
        with self.lock:
            return self._status_locked()

    def _status_locked(self):
        result = None
        if self.board.is_checkmate():
            result = "checkmate"
        elif self.board.is_stalemate():
            result = "stalemate"
        elif self.board.is_insufficient_material():
            result = "draw (insufficient material)"
        elif self.board.can_claim_fifty_moves():
            result = "draw (fifty-move rule)"
        elif self.board.can_claim_threefold_repetition():
            result = "draw (threefold repetition)"

        return {
            "fen": self.board.fen(),
            "turn": "white" if self.board.turn == chess.WHITE else "black",
            "humanColor": "white" if self.human_color == chess.WHITE else "black",
            "inCheck": self.board.is_check(),
            "gameOver": self.board.is_game_over(),
            "result": result,
            "lastMove": self.uci_moves[-1] if self.uci_moves else None,
            "sanHistory": self.san_history,
        }


game = None  # set in main()


class Handler(http.server.BaseHTTPRequestHandler):
    def _json(self, obj, status=200):
        data = json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _serve_static(self, filename, content_type):
        path = os.path.join(STATIC_DIR, filename)
        try:
            with open(path, "rb") as f:
                data = f.read()
        except FileNotFoundError:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _read_json_body(self):
        length = int(self.headers.get("Content-Length", 0) or 0)
        raw = self.rfile.read(length) if length else b""
        return json.loads(raw) if raw else {}

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == "/":
            self._serve_static("index.html", "text/html")
        elif parsed.path == "/api/state":
            self._json(game.status())
        elif parsed.path == "/api/legal_moves":
            qs = urllib.parse.parse_qs(parsed.query)
            square = qs.get("square", [None])[0]
            self._json({"destinations": game.legal_destinations(square)})
        else:
            self.send_error(404)

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        try:
            body = self._read_json_body()
        except json.JSONDecodeError:
            self._json({"error": "bad request body"}, status=400)
            return

        if parsed.path == "/api/new":
            human_color = body.get("humanColor", "white")
            movetime_ms = int(body.get("movetimeMs", 1000))
            self._json(game.new_game(human_color, movetime_ms))
        elif parsed.path == "/api/move":
            uci_str = str(body.get("from", "")) + str(body.get("to", "")) + str(body.get("promotion", ""))
            try:
                self._json(game.human_move(uci_str))
            except ValueError as e:
                self._json({"error": str(e)}, status=400)
        elif parsed.path == "/api/undo":
            self._json(game.undo())
        else:
            self.send_error(404)

    def log_message(self, format_str, *args):
        pass  # keep the console quiet during play


def main():
    ensure_engine_built()

    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765

    engine = Engine(ENGINE_PATH)
    global game
    game = GameState(engine)
    game.new_game("white", 1000)

    with socketserver.ThreadingTCPServer(("127.0.0.1", port), Handler) as server:
        print(f"Playing against: {ENGINE_PATH}")
        print(f"Open http://127.0.0.1:{port} in your browser. Ctrl+C to stop.")
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            engine.quit()


if __name__ == "__main__":
    main()
