#!/usr/bin/env python3

import sys


def fibonacci(n: int) -> int:
    if n < 2:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)


def prime_count(limit: int) -> int:
    count = 0
    for candidate in range(2, limit + 1):
        prime = True
        divisor = 2
        while divisor * divisor <= candidate:
            if candidate % divisor == 0:
                prime = False
                break
            divisor += 1
        if prime:
            count += 1
    return count


def gcd(a: int, b: int) -> int:
    while b != 0:
        a, b = b, a % b
    return a


def gcd_grid(limit: int) -> int:
    checksum = 0
    for row in range(1, limit + 1):
        for column in range(1, limit + 1):
            checksum += gcd(row, column)
    return checksum


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: algorithms.py <fibonacci|prime_count|gcd_grid> <input>")
    algorithm = sys.argv[1]
    value = int(sys.argv[2])
    functions = {
        "fibonacci": fibonacci,
        "prime_count": prime_count,
        "gcd_grid": gcd_grid,
    }
    if algorithm not in functions:
        raise SystemExit(f"unknown algorithm: {algorithm}")
    print(algorithm, functions[algorithm](value))


if __name__ == "__main__":
    main()
