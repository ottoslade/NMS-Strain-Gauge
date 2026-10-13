import argparse
import subprocess
import sys

def run(cmd):
    subprocess.run(cmd, check=True)

def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", required=True, help="uv project path")
    args = parser.parse_args(argv)
    project = args.project
    try:
        # sync uv project to get all python module dependencies
        run(["uv", "sync", "--project", project, "--active"])
        # run type checker
        run(["uv", "run", "ty", "check", "--project", project])
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)

if __name__ == "__main__":
    main()