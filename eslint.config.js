// @ts-check

import eslint from '@eslint/js';
import tseslint from 'typescript-eslint';
import { browser } from "globals";

export default tseslint.config(
  eslint.configs.recommended,
  tseslint.configs.recommended,
  {
    rules: {
      "@typescript-eslint/consistent-type-imports": "error",
      "@typescript-eslint/no-explicit-any": "off",
      "@typescript-eslint/no-unused-vars": "warn",
      "@typescript-eslint/no-unused-expressions": "warn",
      "@typescript-eslint/no-empty-object-type": "warn",
      "@typescript-eslint/triple-slash-reference": "off",
      "@typescript-eslint/no-require-imports": "off"
    },
    languageOptions: {
      globals: {
        ...browser
      }
    }
  }
);