#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <TCanvas.h>
#include <TH1I.h>
#include <TF1.h>
#include <TApplication.h>
#include <TH1.h> // TH1のインクルードを追加

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

// データファイルからヒストグラムを作成する関数
TH1I* createHistogramFromFile(const std::string& filename, bool useCalibration, double conversionFactor) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "エラー: ファイルを開けませんでした " << filename << std::endl;
        return nullptr;
    }

    std::vector<int> counts;
    std::string line;
    bool readingData = false;

    while (std::getline(infile, line)) {
        if (line.find("<<DATA>>") != std::string::npos) {
            readingData = true;
            continue;
        }

        if (readingData && line.find("<<") != std::string::npos && line.find(">>") != std::string::npos) {
            break;
        }

        if (readingData) {
            try {
                counts.push_back(std::stoi(line));
            } catch (const std::invalid_argument& ia) {
                // 数値に変換できない行は無視 (空行など)
            } catch (const std::out_of_range& oor) {
                // 範囲外の数値は無視
            }
        }
    }

    infile.close();

    if (counts.empty()) {
        std::cerr << "エラー: <<DATA>> セクション内にデータが見つかりませんでした。" << std::endl;
        return nullptr;
    }

    int nbins = counts.size();
    std::cout << nbins << " 個のデータポイントを読み込みました。" << std::endl;

    double xlow = 0.0;
    double xhigh = nbins;
    std::string xAxisTitle = "Channel";

    if (useCalibration) {
        xlow = 0.0 * conversionFactor;
        xhigh = nbins * conversionFactor;
        xAxisTitle = "Energy (keV)";
    }

    TH1I *hist = new TH1I("spectrum", ("MCA Spectrum;" + xAxisTitle + ";Counts").c_str(), nbins, xlow, xhigh);
    for (int i = 0; i < nbins; ++i) {
        hist->SetBinContent(i + 1, counts[i]);
    }

    // 統計ボックスを非表示にする
    hist->SetStats(false);

    return hist;
}

void doubleGaussianFit(TH1I* hist) {
    if (!hist) return;

    // ダブルガウシアン関数を定義
    TF1 *fDoubleGauss = new TF1("fDoubleGauss",
                                 "[0]*exp(-0.5*((x-[1])/[2])^2) + [3]*exp(-0.5*((x-[4])/[5])^2)",
                                 300, hist->GetNbinsX());

    // パラメータの初期値を設定 (要調整)
    fDoubleGauss->SetParameters(100, 600, 50, 80, 600, 50); // 例: 振幅1, 平均1, 標準偏差1

    // パラメータに名前を設定
    fDoubleGauss->SetParNames("Amplitude1", "Mean1", "Sigma1", "Amplitude2", "Mean2", "Sigma2");

    // フィットを実行
    hist->Fit("fDoubleGauss", "R");

    // フィット結果を表示
    fDoubleGauss->Print();
}

// TApplication app("myapp", &argc, argv); // TApplicationはグローバルに作成しない

// ファイル名
std::string filename = "Fe55実験データ2/2701V.txt";
std::string calibrationFilename = "fitting.txt";

// キャリブレーションデータを読み込む
std::vector<CalibrationData> calibrations;
if (!loadCalibrationData(calibrationFilename, calibrations)) {
    exit(1);
}

// どのキャリブレーションデータを使用するかを選択
int selectedCalibrationIndex = 0; // 0から始まるインデックス
if (selectedCalibrationIndex < 0 || selectedCalibrationIndex >= calibrations.size()) {
    std::cerr << "エラー: 無効なキャリブレーションインデックスです。最初のキャリブレーションを使用します。" << std::endl;
    selectedCalibrationIndex = 0;
}

// 変換係数を計算
double conversionFactor = calculateConversionFactor(calibrations, selectedCalibrationIndex);
if (conversionFactor <= 0.0) {
    std::cerr << "エラー: 無効な変換係数です。" << std::endl;
    exit(1);
}

// 軸の選択
bool useCalibration = false; // エネルギー較正を使用するかどうかのフラグ

// ヒストグラムを作成
TH1I* hist = createHistogramFromFile(filename, useCalibration, conversionFactor);
if (!hist) {
    exit(1);
}

// ダブルガウシアンフィットを実行
doubleGaussianFit(hist);

// キャンバスを作成してヒストグラムを描画
TCanvas *c1 = new TCanvas("c1", "Spectrum Canvas", 800, 600);
hist->Draw();

// キャンバスを更新して表示を確実に
c1->Update();

// PDFで保存
c1->Print("spectrum_doublegauss.pdf");

// インタラクティブモードで表示
// app.Run(); // ROOTマクロでは必要ありません