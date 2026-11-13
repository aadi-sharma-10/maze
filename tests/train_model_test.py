"""Tests for tools/train_model.py and the dataset program. Run with: make test"""
import os
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools import train_model as train

ROOT = Path(__file__).resolve().parents[1]
DATASET = str(ROOT / "build/maze_dataset")


class TrainingTests(unittest.TestCase):
    def test_seed_groups_do_not_leak(self):
        rows = [{"seed": s, "difficulty": d} for s in range(1, 11) for d in range(3)]
        training, testing = train.split_seeds(rows)
        test_seeds = set(r["seed"] for r in testing)
        train_seeds = set(r["seed"] for r in training)
        self.assertEqual(test_seeds, {5, 10})
        self.assertEqual(train_seeds & test_seeds, set())

    def test_split_needs_train_and_test_seeds(self):
        with self.assertRaises(ValueError):
            train.split_seeds([{"seed": 1}])

    def test_constant_and_duplicate_features(self):
        x = [[i, i, 3] for i in range(10)]
        y = [i / 10 for i in range(10)]
        model = train.fit(x, y)
        self.assertLess(train.mse(model, x, y), 1e-6)
        self.assertEqual(train.predict(model, [-100, -100, 3]), 0)
        self.assertEqual(train.predict(model, [100, 100, 3]), 1)

    def test_c_model_matches_python(self):
        x = [[i, i + 1, i % 3, 2, i % 2, 2 * i, 1 + i / 3] for i in range(20)]
        model = train.fit(x, [i / 20 for i in range(20)])
        with tempfile.TemporaryDirectory() as folder:
            train.export_header(model, Path(folder) / "maze_model.h")
            program = str(Path(folder) / "maze_dataset")
            compiler = shlex.split(os.environ.get("CC", "cc"))
            subprocess.run(compiler + ["-Wall", "-Wextra", "-Werror", "-std=c11",
                                       "-I", str(ROOT / "include"), "-I", folder,
                                       str(ROOT / "src/maze_dataset.c"),
                                       str(ROOT / "src/maze_core.c"),
                                       str(ROOT / "src/maze_generator.c"),
                                       "-lm", "-o", program], check=True)
            probes = x + [[0] * 7, [1000] * 7]
            text = "".join(" ".join(str(v) for v in row) + "\n" for row in probes)
            output = subprocess.check_output([program, "--score"], input=text, text=True)
            lines = output.splitlines()
            self.assertEqual(len(lines), len(probes))
            for row, result in zip(probes, lines):
                self.assertAlmostEqual(float(result), train.predict(model, row), places=12)

    def test_dataset_is_repeatable_and_labels_are_checked(self):
        command = [DATASET, "10", "8", "1"]
        first = subprocess.check_output(command)
        self.assertEqual(first, subprocess.check_output(command))
        with tempfile.TemporaryDirectory() as folder:
            csv_file = Path(folder) / "data.csv"
            csv_file.write_bytes(first)
            rows = train.load_rows(csv_file)
            self.assertEqual(len(rows), 30)
            training, testing = train.split_seeds(rows)
            self.assertEqual(len(training), 24)
            self.assertEqual(len(testing), 6)

            # change the label of the first row to a wrong value (2)
            lines = first.decode().splitlines()
            lines[1] = lines[1].rsplit(",", 1)[0] + ",2"
            csv_file.write_text("\n".join(lines) + "\n")
            with self.assertRaises(ValueError):
                train.load_rows(csv_file)

    def test_bad_dataset_arguments(self):
        for args in (["0"], ["2", "0"], ["2", "8", "4294967295"], ["-1"]):
            result = subprocess.run([DATASET] + args, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b"")


if __name__ == "__main__":
    unittest.main()
