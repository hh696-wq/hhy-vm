<?php

function fibonacci(int $n): int {
    if ($n < 2) {
        return $n;
    }
    return fibonacci($n - 1) + fibonacci($n - 2);
}

function prime_count(int $limit): int {
    $count = 0;
    for ($candidate = 2; $candidate <= $limit; $candidate++) {
        $prime = true;
        for ($divisor = 2; $divisor * $divisor <= $candidate; $divisor++) {
            if ($candidate % $divisor === 0) {
                $prime = false;
                break;
            }
        }
        if ($prime) {
            $count++;
        }
    }
    return $count;
}

function gcd_value(int $a, int $b): int {
    while ($b !== 0) {
        $remainder = $a % $b;
        $a = $b;
        $b = $remainder;
    }
    return $a;
}

function gcd_grid(int $limit): int {
    $checksum = 0;
    for ($row = 1; $row <= $limit; $row++) {
        for ($column = 1; $column <= $limit; $column++) {
            $checksum += gcd_value($row, $column);
        }
    }
    return $checksum;
}

if ($argc !== 3) {
    fwrite(STDERR, "usage: algorithms.php <fibonacci|prime_count|gcd_grid> <input>\n");
    exit(3);
}

$algorithm = $argv[1];
$input = intval($argv[2]);
$result = match ($algorithm) {
    'fibonacci' => fibonacci($input),
    'prime_count' => prime_count($input),
    'gcd_grid' => gcd_grid($input),
    default => null,
};
if ($result === null) {
    fwrite(STDERR, "unknown algorithm: $algorithm\n");
    exit(3);
}
echo "$algorithm $result\n";
