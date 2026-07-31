import tsParser from "@typescript-eslint/parser";

export default [
  {
    ignores: ["dist/", "node_modules/"],
  },
  {
    files: ["**/*.ts"],
    languageOptions: {
      parser: tsParser,
    },
    rules: {
      "no-unused-vars": "warn",
      "no-console": "off",
    },
  },
];
