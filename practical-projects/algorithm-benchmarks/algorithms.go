package main

import (
	"fmt"
	"os"
	"strconv"
)

func fibonacci(n int64) int64 {
	if n < 2 {
		return n
	}
	return fibonacci(n-1) + fibonacci(n-2)
}

func primeCount(limit int64) int64 {
	var count int64
	for candidate := int64(2); candidate <= limit; candidate++ {
		prime := true
		for divisor := int64(2); divisor*divisor <= candidate; divisor++ {
			if candidate%divisor == 0 {
				prime = false
				break
			}
		}
		if prime {
			count++
		}
	}
	return count
}

func gcd(a, b int64) int64 {
	for b != 0 {
		a, b = b, a%b
	}
	return a
}

func gcdGrid(limit int64) int64 {
	var checksum int64
	for row := int64(1); row <= limit; row++ {
		for column := int64(1); column <= limit; column++ {
			checksum += gcd(row, column)
		}
	}
	return checksum
}

func main() {
	if len(os.Args) != 3 {
		fmt.Fprintln(os.Stderr, "usage: algorithms <fibonacci|prime_count|gcd_grid> <input>")
		os.Exit(3)
	}
	input, err := strconv.ParseInt(os.Args[2], 10, 64)
	if err != nil {
		panic(err)
	}
	var result int64
	switch os.Args[1] {
	case "fibonacci":
		result = fibonacci(input)
	case "prime_count":
		result = primeCount(input)
	case "gcd_grid":
		result = gcdGrid(input)
	default:
		fmt.Fprintln(os.Stderr, "unknown algorithm", os.Args[1])
		os.Exit(3)
	}
	fmt.Println(os.Args[1], result)
}
