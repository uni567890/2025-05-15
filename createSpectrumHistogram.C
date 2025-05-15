// createSpectrumHistogram.C

#include <fstream>
#include <vector>
#include <string>
#include <iostream>

#include <TCanvas.h>
#include <TH1I.h>
#include <TROOT.h> // For gROOT if needed, though often implicit

void createSpectrumHistogram() {
    // --- ファイル名 ---
    // アップロードされたファイル名に合わせてください
    std::string filename = "Fe55実験データ2/2601V.txt";

    // --- ファイルを開く ---
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "エラー: ファイルを開けませんでした " << filename << std::endl;
        return;
    }

    std::vector<int> counts;
    std::string line;
    bool readingData = false;

    // --- ファイルを1行ずつ読み込む ---
    while (std::getline(infile, line)) {
        // <<DATA>> セクションの開始を探す
        if (line.find("<<DATA>>") != std::string::npos) {
            readingData = true;
            continue; // <<DATA>> の行自体はスキップ
        }

        // <<END>> または次の <<セクション>> の開始を探す
        // <<DATA>> セクション読み込み中に << で始まり >> で終わる行を見つけたら終了
        if (readingData && line.find("<<") != std::string::npos && line.find(">>") != std::string::npos) {
            break; // データセクションの終了
        }

        // データセクション内であれば、数値を読み込む
        if (readingData) {
            try {
                // 行を整数に変換してvectorに追加
                counts.push_back(std::stoi(line));
            } catch (const std::invalid_argument& ia) {
                // 数値に変換できない行は無視 (空行など)
            } catch (const std::out_of_range& oor) {
                // 範囲外の数値は無視
            }
        }
    }

    // --- ファイルを閉じる ---
    infile.close();

    // --- データが読み込めたか確認 ---
    if (counts.empty()) {
        std::cerr << "エラー: <<DATA>> セクション内にデータが見つかりませんでした。" << std::endl;
        return;
    }

    // --- ビン数を取得 ---
    int nbins = counts.size();
    std::cout << nbins << " 個のデータポイントを読み込みました。" << std::endl;

    // --- ヒストグラムを作成 ---
    TH1I *hist = new TH1I("spectrum", "MCA Spectrum;Channel;Counts", nbins, 0, nbins);

    // --- ヒストグラムにデータを格納 ---
    for (int i = 0; i < nbins; ++i) {
        hist->SetBinContent(i + 1, counts[i]);
    }

    // --- キャンバスを作成してヒストグラムを描画 ---
    TCanvas *c1 = new TCanvas("c1", "Spectrum Canvas", 800, 600);
    hist->Draw();

    // --- ガウスフィット ---
    hist->Fit("gaus", "", "", 0, nbins);

    // --- PDFで保存 ---
    c1->Print("spectrum.pdf");
}