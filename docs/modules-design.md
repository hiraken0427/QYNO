# モジュール設計の採用結果

探索ルートとドット区切りの `import`、`using geometry.circle as circle` を採用し、実装した。

現在の構文・探索規則・制約は [モジュールの仕様](modules.md) を参照。
以前の `using circle = geometry.circle` 案は採用していない。

初期実装は修飾参照、選択 using、全体 using、モジュールの別名、循環・衝突の検査まで。
公開制御（private / export）、相対パス import、複数ルートの設定ファイル、パッケージ管理は今後の検討対象とする。
