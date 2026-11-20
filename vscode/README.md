# QYNO 0.29.8

QYNO用のVisual Studio Code拡張機能である。構文表示、名前の補完、定義への移動、引数の表示を行う。ユーザーが定義した変数や関数も対象となる。funcによる関数型と、関数値の呼び出しにも対応する。libの配列の絞り込みと条件集計を補完する。

async関数とawait、タスクの状態、キャンセル、グループと複数タスクの待機、テキストとバイナリの非同期ファイル操作の補完に対応する。

.qyファイルを開くと有効になる。Visual Studio Code 1.85以降に対応する。セミコロンの後の改行では、インデントを1段増やす。

構文や型のエラー表示には、別途QYNO 0.29.8のqycが必要である。PATHにない場合はqyno.compilerPathで実行ファイルを指定する。検査は信頼済みのローカルワークスペースで行う。ユーザープログラムは実行しない。qyc --checkで検査するため、mainのないモジュールも対象となる。main未定義の警告は編集時に表示しない。

## インストール

拡張機能画面の「VSIXからのインストール」で、同じフォルダのQYNO-0.29.8.vsixを指定する。コマンドからもインストールできる。

```sh
code --install-extension "vscode/QYNO-0.29.8.vsix" --force
```

## ビルド

Node.js 22以降を使用する。

```sh
cd "vscode"
npm install
npm run check
npm run package
```

VSIXはこのフォルダに出力される。コンパイラは同梱しない。

ライセンスはBSD 2-Clause Licenseである。全文はLICENSEに記載している。
