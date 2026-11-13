"""
Trains a small linear model that predicts how often the bots solve a maze.

Train:   python3 tools/train_model.py data/maze_data.csv --output model
Check:   python3 tools/train_model.py data/maze_data.csv --output model --check build/maze_dataset
         (checks that the C version of the model gives the same answers as Python)

Only the Python standard library is used (no numpy / sklearn).
"""
import argparse
import csv
import hashlib
import json
import math
import subprocess
from pathlib import Path

FEATURES = ["vertices", "edges", "dead_ends", "junctions", "loops",
            "shortest_path", "detour_ratio"]
RIDGE = 0.001   # small penalty so the weights don't blow up


def load_rows(path):
    """Read the CSV and check that every row makes sense."""
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))

    seen = set()
    for row in rows:
        row["seed"] = int(row["seed"])
        row["difficulty"] = int(row["difficulty"])

        key = (row["seed"], row["difficulty"])
        if key in seen:
            raise ValueError("Duplicate maze seed/difficulty")
        if row["seed"] <= 0 or row["seed"] > 2**32 - 1:
            raise ValueError("Invalid maze seed")
        if row["difficulty"] not in (0, 1, 2):
            raise ValueError("Invalid difficulty")
        seen.add(key)

        for name in FEATURES + ["success_rate"]:
            row[name] = float(row[name])
            if not math.isfinite(row[name]) or row[name] < 0:
                raise ValueError("Invalid " + name)

        # the label must match the win counts
        trials = int(row["trials_per_bot"])
        random_wins = int(row["random_successes"])
        greedy_wins = int(row["greedy_successes"])
        if trials <= 0:
            raise ValueError("Success label does not match simulation counts")
        if random_wins < 0 or random_wins > trials or greedy_wins < 0 or greedy_wins > trials:
            raise ValueError("Success label does not match simulation counts")
        expected = (random_wins + greedy_wins) / (2 * trials)
        if not math.isclose(row["success_rate"], expected, abs_tol=1e-12):
            raise ValueError("Success label does not match simulation counts")
    return rows


def split_seeds(rows):
    """Every 5th seed goes to the test set (all 3 difficulties of that seed),
    so the same maze seed is never in both train and test."""
    training = []
    testing = []
    for row in rows:
        if row["seed"] % 5 == 0:
            testing.append(row)
        else:
            training.append(row)
    if len(training) == 0 or len(testing) == 0:
        raise ValueError("Dataset must contain both training and held-out seeds")
    return training, testing


def solve(matrix, target):
    """Solve matrix * w = target with Gauss-Jordan elimination."""
    n = len(target)
    a = []
    for i in range(n):
        a.append(matrix[i][:] + [target[i]])

    for col in range(n):
        # pick the row with the biggest number in this column (pivot)
        pivot = col
        for r in range(col + 1, n):
            if abs(a[r][col]) > abs(a[pivot][col]):
                pivot = r
        a[col], a[pivot] = a[pivot], a[col]

        divisor = a[col][col]
        if abs(divisor) < 1e-15:
            raise ValueError("Singular fit")
        a[col] = [value / divisor for value in a[col]]

        # make this column zero in every other row
        for r in range(n):
            if r != col:
                factor = a[r][col]
                a[r] = [a[r][k] - factor * a[col][k] for k in range(n + 1)]

    return [a[i][n] for i in range(n)]


def fit(x, y):
    """Ridge regression on standardised features.
    (we use sum() instead of a loop because sum() adds floats more accurately)"""
    n = len(x)
    width = len(x[0])

    # mean and standard deviation of each feature
    mean = []
    scale = []
    for j in range(width):
        m = sum(row[j] for row in x) / n
        s = math.sqrt(sum((row[j] - m) ** 2 for row in x) / n)
        if s == 0:          # feature never changes
            s = 1.0
        mean.append(m)
        scale.append(s)

    # standardise: (value - mean) / scale
    z = []
    for row in x:
        z.append([(row[j] - mean[j]) / scale[j] for j in range(width)])

    intercept = sum(y) / n

    # normal equations: (Z^T Z / n + ridge * I) w = Z^T (y - intercept) / n
    covariance = []
    for i in range(width):
        line = []
        for j in range(width):
            value = sum(row[i] * row[j] for row in z) / n
            if i == j:
                value = value + RIDGE
            line.append(value)
        covariance.append(line)

    target = []
    for j in range(width):
        target.append(sum(z[k][j] * (y[k] - intercept) for k in range(n)) / n)

    return {"mean": mean, "scale": scale, "weights": solve(covariance, target),
            "intercept": intercept}


def predict(model, row):
    score = model["intercept"]
    for j in range(len(row)):
        score += model["weights"][j] * ((row[j] - model["mean"][j]) / model["scale"][j])
    # success rate must be between 0 and 1
    return min(1.0, max(0.0, score))


def mse(model, x, y):
    return sum((predict(model, x[i]) - y[i]) ** 2 for i in range(len(y))) / len(y)


def export_header(model, path):
    """Write the model as a C header so the C code can use it without Python."""
    def c_array(values):
        return "{" + ", ".join(format(v, ".17g") for v in values) + "}"

    n = len(FEATURES)
    text = "/* maze_model.h - made by tools/train_model.py, don't edit by hand.\n"
    text += "   feature order: " + ", ".join(FEATURES) + " */\n"
    text += "#ifndef MAZE_MODEL_H\n#define MAZE_MODEL_H\n\n"
    text += "#define MODEL_FEATURE_COUNT %d\n\n" % n
    text += "static const double model_mean[%d] = %s;\n" % (n, c_array(model["mean"]))
    text += "static const double model_scale[%d] = %s;\n" % (n, c_array(model["scale"]))
    text += "static const double model_weights[%d] = %s;\n" % (n, c_array(model["weights"]))
    text += "static const double model_intercept = %s;\n\n" % format(model["intercept"], ".17g")
    text += "/* predicted success rate (0 to 1) for the two bots */\n"
    text += "static double predict_success(const double features[MODEL_FEATURE_COUNT])\n"
    text += "{\n"
    text += "    double score = model_intercept;\n"
    text += "    int i;\n"
    text += "    for (i = 0; i < MODEL_FEATURE_COUNT; i++)\n"
    text += "        score += model_weights[i] * ((features[i] - model_mean[i]) / model_scale[i]);\n"
    text += "    if (score < 0.0)\n        score = 0.0;\n"
    text += "    if (score > 1.0)\n        score = 1.0;\n"
    text += "    return score;\n"
    text += "}\n\n#endif\n"
    Path(path).write_text(text)


def check_c_model(rows, model_file, program):
    """Run '<program> --score' and compare every answer with Python."""
    with open(model_file) as f:
        model = json.load(f)
    if model["features"] != FEATURES:
        raise ValueError("Model feature order mismatch")

    vectors = []
    for row in rows:
        vectors.append([row[name] for name in FEATURES])
    # also test the 0 and 1 limits
    vectors.append([0.0] * len(FEATURES))
    vectors.append([1e6] * len(FEATURES))

    text = ""
    for v in vectors:
        text += " ".join(str(value) for value in v) + "\n"
    output = subprocess.check_output([program, "--score"], input=text, text=True)
    scores = [float(line) for line in output.splitlines()]

    if len(scores) != len(vectors):
        raise AssertionError("C program gave the wrong number of scores")
    for i in range(len(vectors)):
        expected = predict(model, vectors[i])
        if not math.isclose(scores[i], expected, rel_tol=1e-12, abs_tol=1e-12):
            raise AssertionError("C/Python score mismatch: %r vs %r" % (scores[i], expected))
    print("C/Python predictions match on %d vectors (tolerance 1e-12)." % len(vectors))


def main():
    parser = argparse.ArgumentParser(description="Train or check the maze difficulty model.")
    parser.add_argument("dataset", type=Path)
    parser.add_argument("--output", type=Path, default=Path("model"))
    parser.add_argument("--check", metavar="PROGRAM",
                        help="only check PROGRAM --score against the saved model")
    args = parser.parse_args()

    rows = load_rows(args.dataset)

    if args.check:
        check_c_model(rows, args.output / "maze_model.json", args.check)
        return

    training, testing = split_seeds(rows)

    def features_of(group):
        return [[r[name] for name in FEATURES] for r in group]

    def baseline_feature_of(group):   # one simple number to compare against
        return [[r["shortest_path"] + r["dead_ends"]] for r in group]

    def labels_of(group):
        return [r["success_rate"] for r in group]

    model = fit(features_of(training), labels_of(training))
    baseline = fit(baseline_feature_of(training), labels_of(training))
    model_error = mse(model, features_of(testing), labels_of(testing))
    baseline_error = mse(baseline, baseline_feature_of(testing), labels_of(testing))

    if model_error < baseline_error:
        verdict = "Model beats baseline"
    else:
        verdict = "Model does not beat baseline"

    report = {
        "dataset_sha256": hashlib.sha256(args.dataset.read_bytes()).hexdigest(),
        "split": "seed % 5 == 0 held out; all difficulties grouped by seed",
        "train_seeds": sorted(set(r["seed"] for r in training)),
        "test_seeds": sorted(set(r["seed"] for r in testing)),
        "train_rows": len(training),
        "test_rows": len(testing),
        "ridge": RIDGE,
        "test_mse": model_error,
        "baseline_test_mse": baseline_error,
        "baseline": {"feature": "shortest_path + dead_ends", "mean": baseline["mean"],
                     "scale": baseline["scale"], "weights": baseline["weights"],
                     "intercept": baseline["intercept"]},
        "verdict": verdict,
    }
    saved_model = {"features": FEATURES, "mean": model["mean"], "scale": model["scale"],
                   "weights": model["weights"], "intercept": model["intercept"]}

    args.output.mkdir(parents=True, exist_ok=True)
    export_header(model, args.output / "maze_model.h")
    (args.output / "maze_model.json").write_text(json.dumps(saved_model, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Held-out MSE: model=%.8f, baseline=%.8f. %s." % (model_error, baseline_error, verdict))


if __name__ == "__main__":
    main()
