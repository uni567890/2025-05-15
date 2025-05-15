// createSpectrumHistogram.C

#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <TCanvas.h>
#include <TH1I.h>
#include <TROOT.h> // For gROOT if needed, though often implicit

// キャリブレーションデータを保持する構造体
struct CalibrationData {
    double channel;
    double energy;
};

// キャリブレーションデータを読み込む関数
bool loadCalibrationData(const std::string& filename, std::vector<CalibrationData>& calibrations) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "エラー: キャリブレーションファイルを開けませんでした: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 空行やコメント行をスキップ
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        CalibrationData data;
        if (ss >> data.channel) {
            // "=" 記号を探す
            std::string eq;
            if (ss >> eq && eq == "=") {
                if (ss >> data.energy) {
                    calibrations.push_back(data);
                } else {
                    std::cerr << "警告: エネルギー値を読み込めませんでした: " << line << std::endl;
                }
            } else {
                std::cerr << "警告: '=' 記号が見つかりませんでした: " << line << std::endl;
            }
        } else {
            std::cerr << "警告: チャンネル値を読み込めませんでした: " << line << std::endl;
        }
    }

    file.close();
    return true;
}

// 選択されたキャリブレーションデータに基づいて変換係数を計算する関数
double calculateConversionFactor(const std::vector<CalibrationData>& calibrations, int selectedIndex) {
    if (selectedIndex < 0 || selectedIndex >= calibrations.size()) {
        std::cerr << "エラー: 無効なキャリブレーションデータのインデックス: " << selectedIndex << std::endl;
        return -1.0; // エラーを示す値を返す
    }

    const CalibrationData& selectedCalibration = calibrations[selectedIndex];
    return selectedCalibration.energy / selectedCalibration.channel;
}

void createSpectrumHistogramcopy() {
    // --- ファイル名 ---
    std::string filename = "Fe55実験データ2/2701V.txt";
    std::string calibrationFilename = "fitting.txt";

    // --- キャリブレーションデータを読み込む ---
    std::vector<CalibrationData> calibrations;
    if (!loadCalibrationData(calibrationFilename, calibrations)) {
        return;
    }

    // --- どのキャリブレーションデータを使用するかを選択 ---
    int selectedCalibrationIndex = 3; // 0から始まるインデックス
    if (selectedCalibrationIndex < 0 || selectedCalibrationIndex >= calibrations.size()) {
        std::cerr << "エラー: 無効なキャリブレーションインデックスです。最初のキャリブレーションを使用します。" << std::endl;
        selectedCalibrationIndex = 0;
    }

    // --- 変換係数を計算 ---
    double conversionFactor = calculateConversionFactor(calibrations, selectedCalibrationIndex);
    if (conversionFactor <= 0.0) {
        std::cerr << "エラー: 無効な変換係数です。" << std::endl;
        return;
    }

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
                // std::cerr << "警告: 無効なデータ形式の行をスキップ: " << line << std::endl;
            } catch (const std::out_of_range& oor) {
                // 範囲外の数値は無視
                // std::cerr << "警告: 範囲外の数値を含む行をスキップ: " << line << std::endl;
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

    // --- エネルギー範囲を計算 ---
    double xlow = 0.0 * conversionFactor;  // 0チャンネルに対応するエネルギー (keV)
    double xhigh = nbins * conversionFactor; // nbinsチャンネルに対応するエネルギー (keV)

    // --- ヒストグラムを作成 ---
    // TH1I(name, title, nbinsx, xlow, xhigh)
    // titleの書式 "Histogram Title;X-axis Title;Y-axis Title"
    // X軸の範囲をeV単位で設定
    TH1I *hist = new TH1I("spectrum", "MCA Spectrum;Energy (keV);Counts", nbins, xlow, xhigh);

    // --- ヒストグラムにデータを格納 ---
    // ROOTのヒストグラムビン番号は1から始まる (1 to nbins)
    // std::vectorのインデックスは0から始まる (0 to nbins-1)
    for (int i = 0; i < nbins; ++i) {
        hist->SetBinContent(i + 1, counts[i]);
    }

    // --- キャンバスを作成してヒストグラムを描画 ---
    TCanvas *c1 = new TCanvas("c1", "Spectrum Canvas", 800, 600);
    hist->Draw();

    // --- 表示範囲を設定 ---
    double xmin = 0.0 * conversionFactor;  // X軸の最小値
    double xmax = nbins * conversionFactor;  // X軸の最大値
    hist->GetXaxis()->SetRangeUser(xmin, xmax);

    // --- ガウスフィットの範囲もエネルギー単位に変換 ---
    double min = 0.0 * conversionFactor; // フィットの最小値
    double max = nbins * conversionFactor; // フィットの最大値
    hist -> Fit("gaus", "", "", 3, 5.3); // ガウスフィットを追加

    // オプション：キャンバスを更新して表示を確実に
    c1->Update();

    c1 -> Print("spectrum.pdf"); // ヒストグラムをpdf形式で保存

    // マクロの実行が終了してもキャンバスは開いたままになります
    // (ROOTインタプリタ内で実行した場合)
}