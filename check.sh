#!/bin/bash

TEST_DIR="tests"
FT_LEX="./ft_lex"
LEX="lex"
CC="gcc"
INPUT_DATA="input.txt"
RESULTS_DIR="results"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RESET='\033[0m'

VERBOSE=false
QUIET=false

while getopts "vq" opt; do
    case $opt in
        v)
            VERBOSE=true
            ;;
        q)
            QUIET=true
            ;;
        \?)
            echo "Usage: $0 [-v] [-q]"
            exit 1
            ;;
    esac
done

mkdir -p "$RESULTS_DIR"

echo "=============================================="
echo "         FT_LEX TESTER                        "
echo "=============================================="

for file in "$TEST_DIR"/*; do
    filename=$(basename "$file")
    base="${filename%.*}"

    echo ""
    echo "🔹 [TEST] $filename"

    $FT_LEX "$file" 2> "$RESULTS_DIR/${base}_ft_lex.err"
    FT_LEX_EXIT=$?

    $LEX "$file" 2> "$RESULTS_DIR/${base}_lex.err"
    LEX_EXIT=$?

    if diff -q "$RESULTS_DIR/${base}_ft_lex.err" "$RESULTS_DIR/${base}_lex.err" >/dev/null; then
        ERR_MATCH=true
    else
        ERR_MATCH=false
    fi

    if [[ $FT_LEX_EXIT -ne 0 && $LEX_EXIT -ne 0 && $ERR_MATCH == true ]]; then
        echo -e "  ✅ ${GREEN}ERROR MATCH: Both failed the same way.${RESET}"
        continue
    fi

    if [[ $FT_LEX_EXIT -ne 0 && $LEX_EXIT -ne 0 ]]; then
        echo -e "  ✅ ${GREEN}ERROR MATCH: Both failed error message differ.${RESET}"
        if [[ $VERBOSE == true ]]; then
            echo "  [ft_lex]"
            echo "$(cat "$RESULTS_DIR/${base}_ft_lex.err")"
            echo "  [lex]"
            echo "$(cat "$RESULTS_DIR/${base}_lex.err")"
        fi
        if [[ $VERBOSE == false ]]; then
            echo -e "  (Use -v flag for detailed error messages)"
        fi
        continue
    fi

    if [[ $FT_LEX_EXIT -ne $LEX_EXIT ]]; then
        echo -e "  ❌ ${RED}FAIL: Exit status mismatch (ft_lex=$FT_LEX_EXIT, lex=$LEX_EXIT)${RESET}"
        continue
    fi

    if [[ $FT_LEX_EXIT -eq 0 && $LEX_EXIT -eq 0 ]]; then
        echo "  🛠️  Both processed successfully. Compiling..."

        $CC lex.yy.c -o "$RESULTS_DIR/${base}_ft_lex_exec" -ll 2> "$RESULTS_DIR/${base}_ft_lex_compile.err"
        FT_LEX_COMPILE_EXIT=$?

        mv lex.yy.c lex.yy.c.lex
        $CC lex.yy.c.lex -o "$RESULTS_DIR/${base}_lex_exec" -ll 2> "$RESULTS_DIR/${base}_lex_compile.err"
        LEX_COMPILE_EXIT=$?

        if [[ $FT_LEX_COMPILE_EXIT -ne 0 || $LEX_COMPILE_EXIT -ne 0 ]]; then
            echo -e "  ❌ ${RED}FAIL: Compilation error${RESET}"
            continue
        fi

        if [[ -f "$INPUT_DATA" ]]; then
            echo "  ▶️ Running executables with input data..."

            "$RESULTS_DIR/${base}_ft_lex_exec" < "$INPUT_DATA" > "$RESULTS_DIR/${base}_ft_lex_output.txt" 2> "$RESULTS_DIR/${base}_ft_lex_runtime.err"
            FT_LEX_RUN_EXIT=$?

            "$RESULTS_DIR/${base}_lex_exec" < "$INPUT_DATA" > "$RESULTS_DIR/${base}_lex_output.txt" 2> "$RESULTS_DIR/${base}_lex_runtime.err"
            LEX_RUN_EXIT=$?

            if ! diff -q "$RESULTS_DIR/${base}_ft_lex_runtime.err" "$RESULTS_DIR/${base}_lex_runtime.err" >/dev/null; then
                echo -e "  ❌ ${RED}FAIL: Runtime error mismatch${RESET}"
                continue
            fi

            if diff -q "$RESULTS_DIR/${base}_ft_lex_output.txt" "$RESULTS_DIR/${base}_lex_output.txt" >/dev/null; then
                echo -e "  ✅ ${GREEN}SUCCESS: Execution outputs match.${RESET}"
            else
                echo -e "  ❌ ${RED}FAIL: Output mismatch${RESET}"
            fi
        else
            echo -e "  ⚠️ ${YELLOW}WARNING: Missing input data file ($INPUT_DATA).${RESET}"
        fi
    fi

    echo -e "  ⚠️ ${YELLOW}UNHANDLED CASE: No specific error detected, but test did not pass.${RESET}"
done

echo ""
echo "=============================================="
echo "          COMPARISON COMPLETE"
echo "=============================================="
