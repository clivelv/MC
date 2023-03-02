/*
    Copyright 2017 Benjamin Vedder	benjamin@vedder.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <cmath>
#include <QFileDialog>
#include <QDir>

namespace {
void truncNum(double &num, double top) {
    if (num > top) {
        num = top;
    } else if (num < -top) {
        num = -top;
    }
}

void svm(float alpha, float beta, uint32_t PWMHalfPeriod,
                uint32_t* tAout, uint32_t* tBout, uint32_t* tCout, uint32_t *svm_sector) {
    uint32_t sector;
    int halfPeriod = PWMHalfPeriod;

    const double ONE_BY_SQRT3 = 0.57735026919;
    const double TWO_BY_SQRT3 = 2.0 * 0.57735026919;

    if (beta >= 0.0f) {
        if (alpha >= 0.0f) {
            //quadrant I
            if (ONE_BY_SQRT3 * beta > alpha) {
                sector = 2;
            } else {
                sector = 1;
            }
        } else {
            //quadrant II
            if (-ONE_BY_SQRT3 * beta > alpha) {
                sector = 3;
            } else {
                sector = 2;
            }
        }
    } else {
        if (alpha >= 0.0f) {
            //quadrant IV5
            if (-ONE_BY_SQRT3 * beta > alpha) {
                sector = 5;
            } else {
                sector = 6;
            }
        } else {
            //quadrant III
            if (ONE_BY_SQRT3 * beta > alpha) {
                sector = 4;
            } else {
                sector = 5;
            }
        }
    }

    // PWM timings
    int tA, tB, tC;

    switch (sector) {

    // sector 1-2
    case 1: {
        // Vector on-times
        int t1 = (alpha - ONE_BY_SQRT3 * beta) * halfPeriod;
        int t2 = (TWO_BY_SQRT3 * beta) * halfPeriod;

        // PWM timings
        tA = (halfPeriod - t1 - t2) / 2;
        tB = tA + t1;
        tC = tB + t2;

        break;
    }

        // sector 2-3
    case 2: {
        // Vector on-times
        int t2 = (alpha + ONE_BY_SQRT3 * beta) * halfPeriod;
        int t3 = (-alpha + ONE_BY_SQRT3 * beta) * halfPeriod;

        // PWM timings
        tB = (halfPeriod - t2 - t3) / 2;
        tA = tB + t3;
        tC = tA + t2;

        break;
    }

        // sector 3-4
    case 3: {
        // Vector on-times
        int t3 = (TWO_BY_SQRT3 * beta) * halfPeriod;
        int t4 = (-alpha - ONE_BY_SQRT3 * beta) * halfPeriod;

        // PWM timings
        tB = (halfPeriod - t3 - t4) / 2;
        tC = tB + t3;
        tA = tC + t4;

        break;
    }

        // sector 4-5
    case 4: {
        // Vector on-times
        int t4 = (-alpha + ONE_BY_SQRT3 * beta) * halfPeriod;
        int t5 = (-TWO_BY_SQRT3 * beta) * halfPeriod;

        // PWM timings
        tC = (halfPeriod - t4 - t5) / 2;
        tB = tC + t5;
        tA = tB + t4;

        break;
    }

        // sector 5-6
    case 5: {
        // Vector on-times
        int t5 = (-alpha - ONE_BY_SQRT3 * beta) * halfPeriod;
        int t6 = (alpha - ONE_BY_SQRT3 * beta) * halfPeriod;

        // PWM timings
        tC = (halfPeriod - t5 - t6) / 2;
        tA = tC + t5;
        tB = tA + t6;

        break;
    }

        // sector 6-1
    case 6: {
        // Vector on-times
        int t6 = (-TWO_BY_SQRT3 * beta) * halfPeriod;
        int t1 = (alpha + ONE_BY_SQRT3 * beta) * halfPeriod;

        // PWM timings
        tA = (halfPeriod - t6 - t1) / 2;
        tC = tA + t1;
        tB = tC + t6;

        break;
    }
    }

    // Truncation
    if (tA <= 0) {
        tA = 0;
    } else if (tA > halfPeriod) {
        tA = halfPeriod;
    }

    if (tB <= 0) {
        tB = 0;
    } else if (tB > halfPeriod) {
        tB = halfPeriod;
    }

    if (tC <= 0) {
        tC = 0;
    } else if (tC > halfPeriod) {
        tC = halfPeriod;
    }

    *tAout = tA;
    *tBout = tB;
    *tCout = tC;
    *svm_sector = sector;
}
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    ui->plot->axisRect()->setRangeZoom(Qt::Horizontal);

    ui->plotPhase->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    ui->plotPhase->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

    ui->plotSvpwm->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    ui->plotSvpwm->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_runButton_clicked()
{
    double mod = ui->modBox->value();
    double current = ui->currentBox->value();// / sqrt(3.0);
    double fsw = ui->fSwBox->value() * 1e3;
    double speed = ui->speedBox->value();
    double revs = ui->revBox->value();
    double tSw = ui->tSwBox->value() * 1e-6;
    double phase_lag = ui->phaseLagBox->value() * (M_PI / 180.0);
    bool sampleV0V7 = ui->sampleV0V7Box->isChecked();
    double maxCurrent = ui->currentMaxBox->value();
    bool twoShunts = ui->twoShuntBox->isChecked();
    bool highCurrentMode = ui->highCurrentBox->isChecked();
    bool hcVoltageCompare = ui->hcVoltageCompareBox->isChecked();
    uint32_t timTop = 10000;

    double cycles = fsw * revs / (speed / 60.0);
    double step_rad = revs * (2.0 * M_PI) / cycles;

    double phase = 0.0;
    double time = 0.0;

    QVector<double> timeList;
    QVector<double> iBusList;

    QVector<double> timeListPhase;
    QVector<double> lia, lib, lic, litot;
    QVector<double> dutyA, dutyB, dutyC, dutyAvg;

    double id = 0.0;
    double iq = current;
    double mod_d = mod * tan(phase_lag);
    double mod_q = mod;

    double i_max = 0.0;

    for (double i = 0;i < cycles;i += 1.0) {
        double s = sin(phase);
        double c = cos(phase);

        if (sampleV0V7) {
            phase += step_rad / 2.0;
        } else {
            phase += step_rad;
        }

        // Inverse Park transform (we assume only D axis current)
        double i_alpha =  c * id - s * iq;
        double i_beta = c * iq + s * id;
        double mod_alpha = c * mod_d - s * mod_q;
        double mod_beta = c * mod_q + s * mod_d;

        // Inverse Clarke transform
        double ia = i_alpha;
        double ib = -0.5 * i_alpha + (sqrt(3.0) / 2.0) * i_beta;
        double ic = -0.5 * i_alpha - (sqrt(3.0) / 2.0) * i_beta;

        if (fabs(ic) > fabs(i_max)) {
            i_max = ic;
        }

        // Do SVM
        uint32_t duty1, duty2, duty3, svmSector;
        svm(-mod_alpha * (sqrt(3.0) / 2.0), -mod_beta * (sqrt(3.0) / 2.0), timTop, &duty1, &duty2, &duty3, &svmSector);
        dutyA.append(duty1);
        dutyB.append(duty2);
        dutyC.append(duty3);
        dutyAvg.append((duty1 + duty2 + duty3) / 3);

        // Update current plot
        double tia = ia;
        double tib = ib;
        double tic = ic;

        truncNum(tia, maxCurrent);
        truncNum(tib, maxCurrent);
        truncNum(tic, maxCurrent);

        // Calculate measured current with sampling after truncation
        if (twoShunts) {
            tic = -(tia + tib);
        } else {
            if (highCurrentMode) {
                double cmp1, cmp2, cmp3;

                if (!hcVoltageCompare) {
                    cmp1 = tia;
                    cmp2 = tib;
                    cmp3 = tic;
                } else {
                    cmp1 = (double)duty1;
                    cmp2 = (double)duty2;
                    cmp3 = (double)duty3;
                    double avg = (cmp1 + cmp2 + cmp3) / 3.0;
                    cmp1 -= avg;
                    cmp2 -= avg;
                    cmp3 -= avg;
                }

                if (fabs(cmp1) > fabs(cmp2) && fabs(cmp1) > fabs(cmp3)) {
                    tia = -(tib + tic);
                } else if (fabs(cmp2) > fabs(cmp1) && fabs(cmp2) > fabs(cmp3)) {
                    tib = -(tia + tic);
                } else if (fabs(cmp3) > fabs(cmp1) && fabs(cmp3) > fabs(cmp2)) {
                    tic = -(tia + tib);
                }
            } else {
                if (duty1 > duty2 && duty1 > duty3) {
                    tia = -(tib + tic);
                } else if (duty2 > duty1 && duty2 > duty3) {
                    tib = -(tia + tic);
                } else if (duty3 > duty1 && duty3 > duty2) {
                    tic = -(tia + tib);
                }
            }
        }

        // Clarke transform assuming balanced currents
        double tialpha = tia;
        double tibeta = (1.0 / sqrt(3.0)) * tia + (2.0 / sqrt(3.0)) * tib;

        // Park transform
        double tid = tialpha * c + tibeta * s;
        double tiq = tibeta * c - tialpha * s;

        litot.append(sqrt(tid * tid + tiq * tiq));
        lia.append(tia);
        lib.append(tib);
        lic.append(tic);
        timeListPhase.append(time);

        // Simulate center-aligned PWM
        for (unsigned int j = 0;j < timTop;j++) {
            bool a = j < duty1;
            bool b = j < duty2;
            bool c = j < duty3;

            double samp = 0.0;
            samp += a ? ia : -ia;
            samp += b ? ib : -ib;
            samp += c ? ic : -ic;

            time += 1.0 / fsw / timTop;
            timeList.append(time);
            iBusList.append(samp);
        }

        // Run contol in the center
        if (sampleV0V7) {
            s = sin(phase);
            c = cos(phase);
            phase += step_rad / 2.0;

            // Inverse Park transform (we assume only D axis current)
            i_alpha =  c * id - s * iq;
            i_beta = c * iq + s * id;
            mod_alpha = c * mod_d - s * mod_q;
            mod_beta = c * mod_q + s * mod_d;

            // Inverse Clarke transform
            ia = i_alpha;
            ib = -0.5 * i_alpha + (sqrt(3.0) / 2.0) * i_beta;
            ic = -0.5 * i_alpha - (sqrt(3.0) / 2.0) * i_beta;

            // Do SVM
            uint32_t duty1, duty2, duty3, svmSector;
            svm(-mod_alpha * (sqrt(3.0) / 2.0), -mod_beta * (sqrt(3.0) / 2.0), timTop, &duty1, &duty2, &duty3, &svmSector);
            dutyA.append(duty1);
            dutyB.append(duty2);
            dutyC.append(duty3);
            dutyAvg.append((duty1 + duty2 + duty3) / 3);

            // Update current plot
            double tia = ia;
            double tib = ib;
            double tic = ic;

            truncNum(tia, maxCurrent);
            truncNum(tib, maxCurrent);
            truncNum(tic, maxCurrent);

            // Calculate measured current with sampling after truncation
            if (twoShunts) {
                tic = -(tia + tib);
            } else {
                if (highCurrentMode) {
                    double cmp1, cmp2, cmp3;

                    if (!hcVoltageCompare) {
                        cmp1 = tia;
                        cmp2 = tib;
                        cmp3 = tic;
                    } else {
                        cmp1 = (double)duty1;
                        cmp2 = (double)duty2;
                        cmp3 = (double)duty3;
                        double avg = (cmp1 + cmp2 + cmp3) / 3.0;
                        cmp1 -= avg;
                        cmp2 -= avg;
                        cmp3 -= avg;
                    }

                    if (fabs(cmp1) > fabs(cmp2) && fabs(cmp1) > fabs(cmp3)) {
                        tia = -(tib + tic);
                    } else if (fabs(cmp2) > fabs(cmp1) && fabs(cmp2) > fabs(cmp3)) {
                        tib = -(tia + tic);
                    } else if (fabs(cmp3) > fabs(cmp1) && fabs(cmp3) > fabs(cmp2)) {
                        tic = -(tia + tib);
                    }
                } else {
                    if (duty1 < duty2 && duty1 < duty3) {
                        tia = -(tib + tic);
                    } else if (duty2 < duty1 && duty2 < duty3) {
                        tib = -(tia + tic);
                    } else if (duty3 < duty1 && duty3 < duty2) {
                        tic = -(tia + tib);
                    }
                }
            }

            // Clarke transform assuming balanced currents
            double tialpha = tia;
            double tibeta = (1.0 / sqrt(3.0)) * tia + (2.0 / sqrt(3.0)) * tib;

            // Park transform
            double tid = tialpha * c + tibeta * s;
            double tiq = tibeta * c - tialpha * s;

            lia.append(tia);
            lib.append(tib);
            lic.append(tic);
            timeListPhase.append(time);
            litot.append(sqrt(tid * tid + tiq * tiq));
        }

        for (unsigned int j = timTop - 1;j > 0;j--) {
            bool a = j < duty1;
            bool b = j < duty2;
            bool c = j < duty3;

            double samp = 0.0;
            samp += a ? ia : -ia;
            samp += b ? ib : -ib;
            samp += c ? ic : -ic;

            time += 1.0 / fsw / timTop;
            timeList.append(time);
            iBusList.append(samp);
        }
    }

    // Reduce data and apply switching times
    mTimeVec.clear();
    mIBusVec.clear();
    for (int i = 0;i < timeList.size();i++) {
        if (i == 0 || i == (timeList.size() - 1)) {
            mTimeVec.append(timeList.at(i));
            mIBusVec.append(iBusList.at(i));
        } else if (iBusList.at(i) != iBusList.at(i - 1) || iBusList.at(i) != iBusList.at(i + 1)) {
            if (iBusList.at(i) != iBusList.at(i - 1)) {
                mTimeVec.append(timeList.at(i - 1) + tSw);
            } else {
                mTimeVec.append(timeList.at(i));
            }

            mIBusVec.append(iBusList.at(i));
        }
    }

    // Calculate RMS current
    double curr_avg = 0.0;
    double t_last = 0.0;
    for (int i = 1;i < mTimeVec.size();i++) {
        double t_diff = mTimeVec.at(i) - t_last;
        t_last = mTimeVec.at(i);
        double curr = (mIBusVec.at(i) + mIBusVec.at(i - 1)) / 2.0;
        curr_avg += curr * t_diff;
    }
    curr_avg /= mTimeVec.last();

    ui->plot->clearGraphs();
    ui->plot->addGraph();
    ui->plot->graph()->setPen(QPen(Qt::black));
    ui->plot->graph()->setData(mTimeVec, mIBusVec);
    ui->plot->graph()->setName(tr("Bus Current"));
    ui->plot->rescaleAxes();
    ui->plot->xAxis->setLabel("Seconds");
    ui->plot->yAxis->setLabel("A");
    ui->plot->legend->setVisible(true);
    ui->plot->replot();

    ui->plotPhase->clearGraphs();
    ui->plotPhase->addGraph();
    ui->plotPhase->graph()->setPen(QPen(Qt::black));
    ui->plotPhase->graph()->setData(timeListPhase, lia);
    ui->plotPhase->graph()->setName(tr("Phase A"));
    ui->plotPhase->addGraph();
    ui->plotPhase->graph()->setPen(QPen(Qt::blue));
    ui->plotPhase->graph()->setData(timeListPhase, lib);
    ui->plotPhase->graph()->setName(tr("Phase B"));
    ui->plotPhase->addGraph();
    ui->plotPhase->graph()->setPen(QPen(Qt::red));
    ui->plotPhase->graph()->setData(timeListPhase, lic);
    ui->plotPhase->graph()->setName(tr("Phase C"));
    ui->plotPhase->addGraph();
    ui->plotPhase->graph()->setPen(QPen(Qt::darkGreen));
    ui->plotPhase->graph()->setData(timeListPhase, litot);
    ui->plotPhase->graph()->setName(tr("Total measured"));
    ui->plotPhase->rescaleAxes();
    ui->plotPhase->xAxis->setLabel("Seconds");
    ui->plotPhase->yAxis->setLabel("A");
    ui->plotPhase->legend->setVisible(true);
    ui->plotPhase->replot();

    ui->plotSvpwm->clearGraphs();
    ui->plotSvpwm->addGraph();
    ui->plotSvpwm->graph()->setPen(QPen(Qt::black));
    ui->plotSvpwm->graph()->setData(timeListPhase, dutyA);
    ui->plotSvpwm->graph()->setName(tr("Phase A"));
    ui->plotSvpwm->addGraph();
    ui->plotSvpwm->graph()->setPen(QPen(Qt::blue));
    ui->plotSvpwm->graph()->setData(timeListPhase, dutyB);
    ui->plotSvpwm->graph()->setName(tr("Phase B"));
    ui->plotSvpwm->addGraph();
    ui->plotSvpwm->graph()->setPen(QPen(Qt::red));
    ui->plotSvpwm->graph()->setData(timeListPhase, dutyC);
    ui->plotSvpwm->graph()->setName(tr("Phase C"));
    ui->plotSvpwm->addGraph();
    ui->plotSvpwm->graph()->setPen(QPen(Qt::darkGreen));
    ui->plotSvpwm->graph()->setData(timeListPhase, dutyAvg);
    ui->plotSvpwm->graph()->setName(tr("Neutral"));
    ui->plotSvpwm->rescaleAxes();
    ui->plotSvpwm->xAxis->setLabel("Seconds");
    ui->plotSvpwm->yAxis->setLabel("Cnt");
    ui->plotSvpwm->legend->setVisible(true);
    ui->plotSvpwm->replot();

    ui->currentLabel->setText(QString("Current RMS: %1 A").arg(curr_avg));
}

void MainWindow::on_actionExport_LTspice_PWL_file_triggered()
{
    QString path;
    path = QFileDialog::getSaveFileName(this,
                                        tr("Choose where to save the LTspice PWL file"),
                                        lastDir,
                                        tr("PWL files (*.txt)"));

    if (path.isNull()) {
        return;
    }

    if (!path.toLower().endsWith(".txt")) {
        path += ".txt";
    }

    lastDir = path;

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);

    for (int i = 0;i < mIBusVec.size();i++) {
        stream << mTimeVec.at(i) << " " << mIBusVec.at(i) << "\n";
    }

    file.close();
}
