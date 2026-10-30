# QYNO 0.27.9

QYNO用のVisual Studio Code拡張機能である。構文表示、名前の補完、定義への移動、引数の表示を行う。ユーザーが定義した変数や関数も対象となる。

.qyファイルを開くと有効になる。Visual Studio Code 1.85以降に対応する。セミコロンの後の改行では、インデントを1段増やす。

構文や型のエラー表示には、別途QYNO 0.27.9のqycが必要である。PATHにない場合はqyno.compilerPathで実行ファイルを指定する。検査は信頼済みのローカルワークスペースで行う。ユーザープログラムは実行しない。

## インストール

拡張機能画面の「VSIXからのインストール」で、同じフォルダのQYNO-0.27.9.vsixを指定する。コマンドからもインストールできる。

```sh
code --install-extension "VSCode Extensions/QYNO-0.27.9.vsix" --force
```

## ビルド

Node.js 22以降を使用する。

```sh
cd "VSCode Extensions"
npm install
npm run check
npm run package
```

VSIXはこのフォルダに出力される。コンパイラは同梱しない。

ライセンスはBSD 2-Clause Licenseである。全文はLICENSEに記載している。
