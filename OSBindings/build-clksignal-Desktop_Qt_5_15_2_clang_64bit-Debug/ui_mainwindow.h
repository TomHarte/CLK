/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "scantargetwidget.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionDo_this;
    ScanTargetWidget *openGLWidget;
    QVBoxLayout *verticalLayout;
    QTabWidget *machineSelectionTabs;
    QWidget *amigaTab;
    QVBoxLayout *vboxLayout;
    QHBoxLayout *hboxLayout;
    QFormLayout *formLayout;
    QLabel *label;
    QComboBox *amigaChipRAMComboBox;
    QLabel *label1;
    QComboBox *amigaFastRAMComboBox;
    QSpacerItem *spacerItem;
    QWidget *appleIITab;
    QVBoxLayout *vboxLayout1;
    QHBoxLayout *hboxLayout1;
    QFormLayout *formLayout1;
    QLabel *label2;
    QComboBox *appleIIModelComboBox;
    QLabel *label3;
    QComboBox *appleIIDiskControllerComboBox;
    QSpacerItem *spacerItem1;
    QWidget *appleIIgsTab;
    QVBoxLayout *vboxLayout2;
    QHBoxLayout *hboxLayout2;
    QFormLayout *formLayout2;
    QLabel *label4;
    QComboBox *appleIIgsModelComboBox;
    QLabel *label5;
    QComboBox *appleIIgsMemorySizeComboBox;
    QSpacerItem *spacerItem2;
    QWidget *amstradCPCTab;
    QVBoxLayout *vboxLayout3;
    QHBoxLayout *hboxLayout3;
    QFormLayout *formLayout3;
    QLabel *label6;
    QComboBox *amstradCPCModelComboBox;
    QSpacerItem *spacerItem3;
    QWidget *atariSTTab;
    QVBoxLayout *vboxLayout4;
    QHBoxLayout *hboxLayout4;
    QFormLayout *formLayout4;
    QLabel *label7;
    QComboBox *atariSTRAMComboBox;
    QWidget *electronTab;
    QVBoxLayout *vboxLayout5;
    QCheckBox *electronDFSCheckBox;
    QCheckBox *electronADFSCheckBox;
    QCheckBox *electronAP6CheckBox;
    QCheckBox *electronSidewaysRAMCheckBox;
    QSpacerItem *spacerItem4;
    QWidget *enterpriseTab;
    QVBoxLayout *vboxLayout6;
    QHBoxLayout *hboxLayout5;
    QFormLayout *formLayout5;
    QLabel *label8;
    QComboBox *enterpriseModelComboBox;
    QLabel *label9;
    QComboBox *enterpriseSpeedComboBox;
    QLabel *label10;
    QComboBox *enterpriseEXOSComboBox;
    QLabel *label11;
    QComboBox *enterpriseBASICComboBox;
    QLabel *label12;
    QComboBox *enterpriseDOSComboBox;
    QSpacerItem *spacerItem5;
    QWidget *macintoshTab;
    QVBoxLayout *vboxLayout7;
    QHBoxLayout *hboxLayout6;
    QFormLayout *formLayout6;
    QLabel *label13;
    QComboBox *macintoshModelComboBox;
    QSpacerItem *spacerItem6;
    QWidget *msxTab;
    QVBoxLayout *vboxLayout8;
    QHBoxLayout *hboxLayout7;
    QFormLayout *formLayout7;
    QLabel *label14;
    QComboBox *msxModelComboBox;
    QSpacerItem *spacerItem7;
    QHBoxLayout *hboxLayout8;
    QFormLayout *formLayout8;
    QLabel *label15;
    QComboBox *msxRegionComboBox;
    QSpacerItem *spacerItem8;
    QCheckBox *msxDiskDriveCheckBox;
    QCheckBox *msxMSXMUSICCheckBox;
    QSpacerItem *spacerItem9;
    QWidget *oricTab;
    QVBoxLayout *vboxLayout9;
    QHBoxLayout *hboxLayout9;
    QFormLayout *formLayout9;
    QLabel *label16;
    QComboBox *oricModelComboBox;
    QLabel *label17;
    QComboBox *oricDiskInterfaceComboBox;
    QSpacerItem *spacerItem10;
    QWidget *pcTab;
    QVBoxLayout *vboxLayout10;
    QHBoxLayout *hboxLayout10;
    QFormLayout *formLayout10;
    QLabel *label18;
    QComboBox *pcVideoAdaptorComboBox;
    QLabel *label19;
    QComboBox *pcSpeedComboBox;
    QSpacerItem *spacerItem11;
    QWidget *vic20Tab;
    QVBoxLayout *vboxLayout11;
    QHBoxLayout *hboxLayout11;
    QFormLayout *formLayout11;
    QLabel *label20;
    QComboBox *vic20RegionComboBox;
    QLabel *label21;
    QComboBox *vic20MemorySizeComboBox;
    QSpacerItem *spacerItem12;
    QCheckBox *vic20C1540CheckBox;
    QSpacerItem *spacerItem13;
    QWidget *zx80Tab;
    QVBoxLayout *vboxLayout12;
    QHBoxLayout *hboxLayout12;
    QFormLayout *formLayout12;
    QLabel *label22;
    QComboBox *zx80MemorySizeComboBox;
    QSpacerItem *spacerItem14;
    QCheckBox *zx80UseZX81ROMCheckBox;
    QSpacerItem *spacerItem15;
    QWidget *zx81Tab;
    QVBoxLayout *vboxLayout13;
    QHBoxLayout *hboxLayout13;
    QFormLayout *formLayout13;
    QLabel *label23;
    QComboBox *zx81MemorySizeComboBox;
    QSpacerItem *spacerItem16;
    QWidget *spectrumTab;
    QVBoxLayout *vboxLayout14;
    QHBoxLayout *hboxLayout14;
    QFormLayout *formLayout14;
    QLabel *label24;
    QComboBox *spectrumModelComboBox;
    QSpacerItem *spacerItem17;
    QLabel *topTipLabel;
    QPushButton *startMachineButton;
    QPlainTextEdit *missingROMsBox;
    QSlider *volumeSlider;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(800, 600);
        MainWindow->setMinimumSize(QSize(400, 300));
        MainWindow->setAcceptDrops(true);
        actionDo_this = new QAction(MainWindow);
        actionDo_this->setObjectName(QString::fromUtf8("actionDo_this"));
        openGLWidget = new ScanTargetWidget(MainWindow);
        openGLWidget->setObjectName(QString::fromUtf8("openGLWidget"));
        verticalLayout = new QVBoxLayout(openGLWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        machineSelectionTabs = new QTabWidget(openGLWidget);
        machineSelectionTabs->setObjectName(QString::fromUtf8("machineSelectionTabs"));
        amigaTab = new QWidget();
        amigaTab->setObjectName(QString::fromUtf8("amigaTab"));
        vboxLayout = new QVBoxLayout(amigaTab);
        vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
        hboxLayout = new QHBoxLayout();
        hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
        formLayout = new QFormLayout();
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        label = new QLabel(amigaTab);
        label->setObjectName(QString::fromUtf8("label"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        amigaChipRAMComboBox = new QComboBox(amigaTab);
        amigaChipRAMComboBox->addItem(QString());
        amigaChipRAMComboBox->addItem(QString());
        amigaChipRAMComboBox->addItem(QString());
        amigaChipRAMComboBox->setObjectName(QString::fromUtf8("amigaChipRAMComboBox"));

        formLayout->setWidget(0, QFormLayout::FieldRole, amigaChipRAMComboBox);

        label1 = new QLabel(amigaTab);
        label1->setObjectName(QString::fromUtf8("label1"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label1);

        amigaFastRAMComboBox = new QComboBox(amigaTab);
        amigaFastRAMComboBox->addItem(QString());
        amigaFastRAMComboBox->addItem(QString());
        amigaFastRAMComboBox->addItem(QString());
        amigaFastRAMComboBox->addItem(QString());
        amigaFastRAMComboBox->addItem(QString());
        amigaFastRAMComboBox->setObjectName(QString::fromUtf8("amigaFastRAMComboBox"));

        formLayout->setWidget(1, QFormLayout::FieldRole, amigaFastRAMComboBox);


        hboxLayout->addLayout(formLayout);

        spacerItem = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout->addItem(spacerItem);


        vboxLayout->addLayout(hboxLayout);

        machineSelectionTabs->addTab(amigaTab, QString());
        appleIITab = new QWidget();
        appleIITab->setObjectName(QString::fromUtf8("appleIITab"));
        vboxLayout1 = new QVBoxLayout(appleIITab);
        vboxLayout1->setObjectName(QString::fromUtf8("vboxLayout1"));
        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName(QString::fromUtf8("hboxLayout1"));
        formLayout1 = new QFormLayout();
        formLayout1->setObjectName(QString::fromUtf8("formLayout1"));
        label2 = new QLabel(appleIITab);
        label2->setObjectName(QString::fromUtf8("label2"));

        formLayout1->setWidget(0, QFormLayout::LabelRole, label2);

        appleIIModelComboBox = new QComboBox(appleIITab);
        appleIIModelComboBox->addItem(QString());
        appleIIModelComboBox->addItem(QString());
        appleIIModelComboBox->addItem(QString());
        appleIIModelComboBox->addItem(QString());
        appleIIModelComboBox->setObjectName(QString::fromUtf8("appleIIModelComboBox"));

        formLayout1->setWidget(0, QFormLayout::FieldRole, appleIIModelComboBox);

        label3 = new QLabel(appleIITab);
        label3->setObjectName(QString::fromUtf8("label3"));

        formLayout1->setWidget(1, QFormLayout::LabelRole, label3);

        appleIIDiskControllerComboBox = new QComboBox(appleIITab);
        appleIIDiskControllerComboBox->addItem(QString());
        appleIIDiskControllerComboBox->addItem(QString());
        appleIIDiskControllerComboBox->addItem(QString());
        appleIIDiskControllerComboBox->setObjectName(QString::fromUtf8("appleIIDiskControllerComboBox"));

        formLayout1->setWidget(1, QFormLayout::FieldRole, appleIIDiskControllerComboBox);


        hboxLayout1->addLayout(formLayout1);

        spacerItem1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout1->addItem(spacerItem1);


        vboxLayout1->addLayout(hboxLayout1);

        machineSelectionTabs->addTab(appleIITab, QString());
        appleIIgsTab = new QWidget();
        appleIIgsTab->setObjectName(QString::fromUtf8("appleIIgsTab"));
        vboxLayout2 = new QVBoxLayout(appleIIgsTab);
        vboxLayout2->setObjectName(QString::fromUtf8("vboxLayout2"));
        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName(QString::fromUtf8("hboxLayout2"));
        formLayout2 = new QFormLayout();
        formLayout2->setObjectName(QString::fromUtf8("formLayout2"));
        label4 = new QLabel(appleIIgsTab);
        label4->setObjectName(QString::fromUtf8("label4"));

        formLayout2->setWidget(0, QFormLayout::LabelRole, label4);

        appleIIgsModelComboBox = new QComboBox(appleIIgsTab);
        appleIIgsModelComboBox->addItem(QString());
        appleIIgsModelComboBox->addItem(QString());
        appleIIgsModelComboBox->addItem(QString());
        appleIIgsModelComboBox->setObjectName(QString::fromUtf8("appleIIgsModelComboBox"));

        formLayout2->setWidget(0, QFormLayout::FieldRole, appleIIgsModelComboBox);

        label5 = new QLabel(appleIIgsTab);
        label5->setObjectName(QString::fromUtf8("label5"));

        formLayout2->setWidget(1, QFormLayout::LabelRole, label5);

        appleIIgsMemorySizeComboBox = new QComboBox(appleIIgsTab);
        appleIIgsMemorySizeComboBox->addItem(QString());
        appleIIgsMemorySizeComboBox->addItem(QString());
        appleIIgsMemorySizeComboBox->addItem(QString());
        appleIIgsMemorySizeComboBox->setObjectName(QString::fromUtf8("appleIIgsMemorySizeComboBox"));

        formLayout2->setWidget(1, QFormLayout::FieldRole, appleIIgsMemorySizeComboBox);


        hboxLayout2->addLayout(formLayout2);

        spacerItem2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout2->addItem(spacerItem2);


        vboxLayout2->addLayout(hboxLayout2);

        machineSelectionTabs->addTab(appleIIgsTab, QString());
        amstradCPCTab = new QWidget();
        amstradCPCTab->setObjectName(QString::fromUtf8("amstradCPCTab"));
        vboxLayout3 = new QVBoxLayout(amstradCPCTab);
        vboxLayout3->setObjectName(QString::fromUtf8("vboxLayout3"));
        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName(QString::fromUtf8("hboxLayout3"));
        formLayout3 = new QFormLayout();
        formLayout3->setObjectName(QString::fromUtf8("formLayout3"));
        label6 = new QLabel(amstradCPCTab);
        label6->setObjectName(QString::fromUtf8("label6"));

        formLayout3->setWidget(0, QFormLayout::LabelRole, label6);

        amstradCPCModelComboBox = new QComboBox(amstradCPCTab);
        amstradCPCModelComboBox->addItem(QString());
        amstradCPCModelComboBox->addItem(QString());
        amstradCPCModelComboBox->addItem(QString());
        amstradCPCModelComboBox->setObjectName(QString::fromUtf8("amstradCPCModelComboBox"));

        formLayout3->setWidget(0, QFormLayout::FieldRole, amstradCPCModelComboBox);


        hboxLayout3->addLayout(formLayout3);

        spacerItem3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout3->addItem(spacerItem3);


        vboxLayout3->addLayout(hboxLayout3);

        machineSelectionTabs->addTab(amstradCPCTab, QString());
        atariSTTab = new QWidget();
        atariSTTab->setObjectName(QString::fromUtf8("atariSTTab"));
        vboxLayout4 = new QVBoxLayout(atariSTTab);
        vboxLayout4->setObjectName(QString::fromUtf8("vboxLayout4"));
        hboxLayout4 = new QHBoxLayout();
        hboxLayout4->setObjectName(QString::fromUtf8("hboxLayout4"));
        formLayout4 = new QFormLayout();
        formLayout4->setObjectName(QString::fromUtf8("formLayout4"));
        label7 = new QLabel(atariSTTab);
        label7->setObjectName(QString::fromUtf8("label7"));

        formLayout4->setWidget(0, QFormLayout::LabelRole, label7);

        atariSTRAMComboBox = new QComboBox(atariSTTab);
        atariSTRAMComboBox->addItem(QString());
        atariSTRAMComboBox->addItem(QString());
        atariSTRAMComboBox->addItem(QString());
        atariSTRAMComboBox->setObjectName(QString::fromUtf8("atariSTRAMComboBox"));

        formLayout4->setWidget(0, QFormLayout::FieldRole, atariSTRAMComboBox);


        hboxLayout4->addLayout(formLayout4);


        vboxLayout4->addLayout(hboxLayout4);

        machineSelectionTabs->addTab(atariSTTab, QString());
        electronTab = new QWidget();
        electronTab->setObjectName(QString::fromUtf8("electronTab"));
        vboxLayout5 = new QVBoxLayout(electronTab);
        vboxLayout5->setObjectName(QString::fromUtf8("vboxLayout5"));
        electronDFSCheckBox = new QCheckBox(electronTab);
        electronDFSCheckBox->setObjectName(QString::fromUtf8("electronDFSCheckBox"));

        vboxLayout5->addWidget(electronDFSCheckBox);

        electronADFSCheckBox = new QCheckBox(electronTab);
        electronADFSCheckBox->setObjectName(QString::fromUtf8("electronADFSCheckBox"));

        vboxLayout5->addWidget(electronADFSCheckBox);

        electronAP6CheckBox = new QCheckBox(electronTab);
        electronAP6CheckBox->setObjectName(QString::fromUtf8("electronAP6CheckBox"));

        vboxLayout5->addWidget(electronAP6CheckBox);

        electronSidewaysRAMCheckBox = new QCheckBox(electronTab);
        electronSidewaysRAMCheckBox->setObjectName(QString::fromUtf8("electronSidewaysRAMCheckBox"));

        vboxLayout5->addWidget(electronSidewaysRAMCheckBox);

        spacerItem4 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        vboxLayout5->addItem(spacerItem4);

        machineSelectionTabs->addTab(electronTab, QString());
        enterpriseTab = new QWidget();
        enterpriseTab->setObjectName(QString::fromUtf8("enterpriseTab"));
        vboxLayout6 = new QVBoxLayout(enterpriseTab);
        vboxLayout6->setObjectName(QString::fromUtf8("vboxLayout6"));
        hboxLayout5 = new QHBoxLayout();
        hboxLayout5->setObjectName(QString::fromUtf8("hboxLayout5"));
        formLayout5 = new QFormLayout();
        formLayout5->setObjectName(QString::fromUtf8("formLayout5"));
        label8 = new QLabel(enterpriseTab);
        label8->setObjectName(QString::fromUtf8("label8"));

        formLayout5->setWidget(0, QFormLayout::LabelRole, label8);

        enterpriseModelComboBox = new QComboBox(enterpriseTab);
        enterpriseModelComboBox->addItem(QString());
        enterpriseModelComboBox->addItem(QString());
        enterpriseModelComboBox->addItem(QString());
        enterpriseModelComboBox->setObjectName(QString::fromUtf8("enterpriseModelComboBox"));

        formLayout5->setWidget(0, QFormLayout::FieldRole, enterpriseModelComboBox);

        label9 = new QLabel(enterpriseTab);
        label9->setObjectName(QString::fromUtf8("label9"));

        formLayout5->setWidget(1, QFormLayout::LabelRole, label9);

        enterpriseSpeedComboBox = new QComboBox(enterpriseTab);
        enterpriseSpeedComboBox->addItem(QString());
        enterpriseSpeedComboBox->addItem(QString());
        enterpriseSpeedComboBox->setObjectName(QString::fromUtf8("enterpriseSpeedComboBox"));

        formLayout5->setWidget(1, QFormLayout::FieldRole, enterpriseSpeedComboBox);

        label10 = new QLabel(enterpriseTab);
        label10->setObjectName(QString::fromUtf8("label10"));

        formLayout5->setWidget(2, QFormLayout::LabelRole, label10);

        enterpriseEXOSComboBox = new QComboBox(enterpriseTab);
        enterpriseEXOSComboBox->addItem(QString());
        enterpriseEXOSComboBox->addItem(QString());
        enterpriseEXOSComboBox->addItem(QString());
        enterpriseEXOSComboBox->setObjectName(QString::fromUtf8("enterpriseEXOSComboBox"));

        formLayout5->setWidget(2, QFormLayout::FieldRole, enterpriseEXOSComboBox);

        label11 = new QLabel(enterpriseTab);
        label11->setObjectName(QString::fromUtf8("label11"));

        formLayout5->setWidget(3, QFormLayout::LabelRole, label11);

        enterpriseBASICComboBox = new QComboBox(enterpriseTab);
        enterpriseBASICComboBox->addItem(QString());
        enterpriseBASICComboBox->addItem(QString());
        enterpriseBASICComboBox->addItem(QString());
        enterpriseBASICComboBox->addItem(QString());
        enterpriseBASICComboBox->setObjectName(QString::fromUtf8("enterpriseBASICComboBox"));

        formLayout5->setWidget(3, QFormLayout::FieldRole, enterpriseBASICComboBox);

        label12 = new QLabel(enterpriseTab);
        label12->setObjectName(QString::fromUtf8("label12"));

        formLayout5->setWidget(4, QFormLayout::LabelRole, label12);

        enterpriseDOSComboBox = new QComboBox(enterpriseTab);
        enterpriseDOSComboBox->addItem(QString());
        enterpriseDOSComboBox->addItem(QString());
        enterpriseDOSComboBox->setObjectName(QString::fromUtf8("enterpriseDOSComboBox"));

        formLayout5->setWidget(4, QFormLayout::FieldRole, enterpriseDOSComboBox);


        hboxLayout5->addLayout(formLayout5);

        spacerItem5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout5->addItem(spacerItem5);


        vboxLayout6->addLayout(hboxLayout5);

        machineSelectionTabs->addTab(enterpriseTab, QString());
        macintoshTab = new QWidget();
        macintoshTab->setObjectName(QString::fromUtf8("macintoshTab"));
        vboxLayout7 = new QVBoxLayout(macintoshTab);
        vboxLayout7->setObjectName(QString::fromUtf8("vboxLayout7"));
        hboxLayout6 = new QHBoxLayout();
        hboxLayout6->setObjectName(QString::fromUtf8("hboxLayout6"));
        formLayout6 = new QFormLayout();
        formLayout6->setObjectName(QString::fromUtf8("formLayout6"));
        label13 = new QLabel(macintoshTab);
        label13->setObjectName(QString::fromUtf8("label13"));

        formLayout6->setWidget(0, QFormLayout::LabelRole, label13);

        macintoshModelComboBox = new QComboBox(macintoshTab);
        macintoshModelComboBox->addItem(QString());
        macintoshModelComboBox->addItem(QString());
        macintoshModelComboBox->addItem(QString());
        macintoshModelComboBox->addItem(QString());
        macintoshModelComboBox->setObjectName(QString::fromUtf8("macintoshModelComboBox"));

        formLayout6->setWidget(0, QFormLayout::FieldRole, macintoshModelComboBox);


        hboxLayout6->addLayout(formLayout6);

        spacerItem6 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout6->addItem(spacerItem6);


        vboxLayout7->addLayout(hboxLayout6);

        machineSelectionTabs->addTab(macintoshTab, QString());
        msxTab = new QWidget();
        msxTab->setObjectName(QString::fromUtf8("msxTab"));
        vboxLayout8 = new QVBoxLayout(msxTab);
        vboxLayout8->setObjectName(QString::fromUtf8("vboxLayout8"));
        hboxLayout7 = new QHBoxLayout();
        hboxLayout7->setObjectName(QString::fromUtf8("hboxLayout7"));
        formLayout7 = new QFormLayout();
        formLayout7->setObjectName(QString::fromUtf8("formLayout7"));
        label14 = new QLabel(msxTab);
        label14->setObjectName(QString::fromUtf8("label14"));

        formLayout7->setWidget(0, QFormLayout::LabelRole, label14);

        msxModelComboBox = new QComboBox(msxTab);
        msxModelComboBox->addItem(QString());
        msxModelComboBox->addItem(QString());
        msxModelComboBox->setObjectName(QString::fromUtf8("msxModelComboBox"));

        formLayout7->setWidget(0, QFormLayout::FieldRole, msxModelComboBox);


        hboxLayout7->addLayout(formLayout7);

        spacerItem7 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout7->addItem(spacerItem7);


        vboxLayout8->addLayout(hboxLayout7);

        hboxLayout8 = new QHBoxLayout();
        hboxLayout8->setObjectName(QString::fromUtf8("hboxLayout8"));
        formLayout8 = new QFormLayout();
        formLayout8->setObjectName(QString::fromUtf8("formLayout8"));
        label15 = new QLabel(msxTab);
        label15->setObjectName(QString::fromUtf8("label15"));

        formLayout8->setWidget(0, QFormLayout::LabelRole, label15);

        msxRegionComboBox = new QComboBox(msxTab);
        msxRegionComboBox->addItem(QString());
        msxRegionComboBox->addItem(QString());
        msxRegionComboBox->addItem(QString());
        msxRegionComboBox->setObjectName(QString::fromUtf8("msxRegionComboBox"));

        formLayout8->setWidget(0, QFormLayout::FieldRole, msxRegionComboBox);


        hboxLayout8->addLayout(formLayout8);

        spacerItem8 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout8->addItem(spacerItem8);


        vboxLayout8->addLayout(hboxLayout8);

        msxDiskDriveCheckBox = new QCheckBox(msxTab);
        msxDiskDriveCheckBox->setObjectName(QString::fromUtf8("msxDiskDriveCheckBox"));

        vboxLayout8->addWidget(msxDiskDriveCheckBox);

        msxMSXMUSICCheckBox = new QCheckBox(msxTab);
        msxMSXMUSICCheckBox->setObjectName(QString::fromUtf8("msxMSXMUSICCheckBox"));

        vboxLayout8->addWidget(msxMSXMUSICCheckBox);

        spacerItem9 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        vboxLayout8->addItem(spacerItem9);

        machineSelectionTabs->addTab(msxTab, QString());
        oricTab = new QWidget();
        oricTab->setObjectName(QString::fromUtf8("oricTab"));
        vboxLayout9 = new QVBoxLayout(oricTab);
        vboxLayout9->setObjectName(QString::fromUtf8("vboxLayout9"));
        hboxLayout9 = new QHBoxLayout();
        hboxLayout9->setObjectName(QString::fromUtf8("hboxLayout9"));
        formLayout9 = new QFormLayout();
        formLayout9->setObjectName(QString::fromUtf8("formLayout9"));
        label16 = new QLabel(oricTab);
        label16->setObjectName(QString::fromUtf8("label16"));

        formLayout9->setWidget(0, QFormLayout::LabelRole, label16);

        oricModelComboBox = new QComboBox(oricTab);
        oricModelComboBox->addItem(QString());
        oricModelComboBox->addItem(QString());
        oricModelComboBox->addItem(QString());
        oricModelComboBox->setObjectName(QString::fromUtf8("oricModelComboBox"));

        formLayout9->setWidget(0, QFormLayout::FieldRole, oricModelComboBox);

        label17 = new QLabel(oricTab);
        label17->setObjectName(QString::fromUtf8("label17"));

        formLayout9->setWidget(1, QFormLayout::LabelRole, label17);

        oricDiskInterfaceComboBox = new QComboBox(oricTab);
        oricDiskInterfaceComboBox->addItem(QString());
        oricDiskInterfaceComboBox->addItem(QString());
        oricDiskInterfaceComboBox->addItem(QString());
        oricDiskInterfaceComboBox->addItem(QString());
        oricDiskInterfaceComboBox->addItem(QString());
        oricDiskInterfaceComboBox->setObjectName(QString::fromUtf8("oricDiskInterfaceComboBox"));

        formLayout9->setWidget(1, QFormLayout::FieldRole, oricDiskInterfaceComboBox);


        hboxLayout9->addLayout(formLayout9);

        spacerItem10 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout9->addItem(spacerItem10);


        vboxLayout9->addLayout(hboxLayout9);

        machineSelectionTabs->addTab(oricTab, QString());
        pcTab = new QWidget();
        pcTab->setObjectName(QString::fromUtf8("pcTab"));
        vboxLayout10 = new QVBoxLayout(pcTab);
        vboxLayout10->setObjectName(QString::fromUtf8("vboxLayout10"));
        hboxLayout10 = new QHBoxLayout();
        hboxLayout10->setObjectName(QString::fromUtf8("hboxLayout10"));
        formLayout10 = new QFormLayout();
        formLayout10->setObjectName(QString::fromUtf8("formLayout10"));
        label18 = new QLabel(pcTab);
        label18->setObjectName(QString::fromUtf8("label18"));

        formLayout10->setWidget(0, QFormLayout::LabelRole, label18);

        pcVideoAdaptorComboBox = new QComboBox(pcTab);
        pcVideoAdaptorComboBox->addItem(QString());
        pcVideoAdaptorComboBox->addItem(QString());
        pcVideoAdaptorComboBox->setObjectName(QString::fromUtf8("pcVideoAdaptorComboBox"));

        formLayout10->setWidget(0, QFormLayout::FieldRole, pcVideoAdaptorComboBox);

        label19 = new QLabel(pcTab);
        label19->setObjectName(QString::fromUtf8("label19"));

        formLayout10->setWidget(1, QFormLayout::LabelRole, label19);

        pcSpeedComboBox = new QComboBox(pcTab);
        pcSpeedComboBox->addItem(QString());
        pcSpeedComboBox->addItem(QString());
        pcSpeedComboBox->setObjectName(QString::fromUtf8("pcSpeedComboBox"));

        formLayout10->setWidget(1, QFormLayout::FieldRole, pcSpeedComboBox);


        hboxLayout10->addLayout(formLayout10);

        spacerItem11 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout10->addItem(spacerItem11);


        vboxLayout10->addLayout(hboxLayout10);

        machineSelectionTabs->addTab(pcTab, QString());
        vic20Tab = new QWidget();
        vic20Tab->setObjectName(QString::fromUtf8("vic20Tab"));
        vboxLayout11 = new QVBoxLayout(vic20Tab);
        vboxLayout11->setObjectName(QString::fromUtf8("vboxLayout11"));
        hboxLayout11 = new QHBoxLayout();
        hboxLayout11->setObjectName(QString::fromUtf8("hboxLayout11"));
        formLayout11 = new QFormLayout();
        formLayout11->setObjectName(QString::fromUtf8("formLayout11"));
        label20 = new QLabel(vic20Tab);
        label20->setObjectName(QString::fromUtf8("label20"));

        formLayout11->setWidget(0, QFormLayout::LabelRole, label20);

        vic20RegionComboBox = new QComboBox(vic20Tab);
        vic20RegionComboBox->addItem(QString());
        vic20RegionComboBox->addItem(QString());
        vic20RegionComboBox->addItem(QString());
        vic20RegionComboBox->addItem(QString());
        vic20RegionComboBox->addItem(QString());
        vic20RegionComboBox->setObjectName(QString::fromUtf8("vic20RegionComboBox"));

        formLayout11->setWidget(0, QFormLayout::FieldRole, vic20RegionComboBox);

        label21 = new QLabel(vic20Tab);
        label21->setObjectName(QString::fromUtf8("label21"));

        formLayout11->setWidget(1, QFormLayout::LabelRole, label21);

        vic20MemorySizeComboBox = new QComboBox(vic20Tab);
        vic20MemorySizeComboBox->addItem(QString());
        vic20MemorySizeComboBox->addItem(QString());
        vic20MemorySizeComboBox->addItem(QString());
        vic20MemorySizeComboBox->setObjectName(QString::fromUtf8("vic20MemorySizeComboBox"));

        formLayout11->setWidget(1, QFormLayout::FieldRole, vic20MemorySizeComboBox);


        hboxLayout11->addLayout(formLayout11);

        spacerItem12 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout11->addItem(spacerItem12);


        vboxLayout11->addLayout(hboxLayout11);

        vic20C1540CheckBox = new QCheckBox(vic20Tab);
        vic20C1540CheckBox->setObjectName(QString::fromUtf8("vic20C1540CheckBox"));

        vboxLayout11->addWidget(vic20C1540CheckBox);

        spacerItem13 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        vboxLayout11->addItem(spacerItem13);

        machineSelectionTabs->addTab(vic20Tab, QString());
        zx80Tab = new QWidget();
        zx80Tab->setObjectName(QString::fromUtf8("zx80Tab"));
        vboxLayout12 = new QVBoxLayout(zx80Tab);
        vboxLayout12->setObjectName(QString::fromUtf8("vboxLayout12"));
        hboxLayout12 = new QHBoxLayout();
        hboxLayout12->setObjectName(QString::fromUtf8("hboxLayout12"));
        formLayout12 = new QFormLayout();
        formLayout12->setObjectName(QString::fromUtf8("formLayout12"));
        label22 = new QLabel(zx80Tab);
        label22->setObjectName(QString::fromUtf8("label22"));

        formLayout12->setWidget(0, QFormLayout::LabelRole, label22);

        zx80MemorySizeComboBox = new QComboBox(zx80Tab);
        zx80MemorySizeComboBox->addItem(QString());
        zx80MemorySizeComboBox->addItem(QString());
        zx80MemorySizeComboBox->setObjectName(QString::fromUtf8("zx80MemorySizeComboBox"));

        formLayout12->setWidget(0, QFormLayout::FieldRole, zx80MemorySizeComboBox);


        hboxLayout12->addLayout(formLayout12);

        spacerItem14 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout12->addItem(spacerItem14);


        vboxLayout12->addLayout(hboxLayout12);

        zx80UseZX81ROMCheckBox = new QCheckBox(zx80Tab);
        zx80UseZX81ROMCheckBox->setObjectName(QString::fromUtf8("zx80UseZX81ROMCheckBox"));

        vboxLayout12->addWidget(zx80UseZX81ROMCheckBox);

        spacerItem15 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        vboxLayout12->addItem(spacerItem15);

        machineSelectionTabs->addTab(zx80Tab, QString());
        zx81Tab = new QWidget();
        zx81Tab->setObjectName(QString::fromUtf8("zx81Tab"));
        vboxLayout13 = new QVBoxLayout(zx81Tab);
        vboxLayout13->setObjectName(QString::fromUtf8("vboxLayout13"));
        hboxLayout13 = new QHBoxLayout();
        hboxLayout13->setObjectName(QString::fromUtf8("hboxLayout13"));
        formLayout13 = new QFormLayout();
        formLayout13->setObjectName(QString::fromUtf8("formLayout13"));
        label23 = new QLabel(zx81Tab);
        label23->setObjectName(QString::fromUtf8("label23"));

        formLayout13->setWidget(0, QFormLayout::LabelRole, label23);

        zx81MemorySizeComboBox = new QComboBox(zx81Tab);
        zx81MemorySizeComboBox->addItem(QString());
        zx81MemorySizeComboBox->addItem(QString());
        zx81MemorySizeComboBox->setObjectName(QString::fromUtf8("zx81MemorySizeComboBox"));

        formLayout13->setWidget(0, QFormLayout::FieldRole, zx81MemorySizeComboBox);


        hboxLayout13->addLayout(formLayout13);

        spacerItem16 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout13->addItem(spacerItem16);


        vboxLayout13->addLayout(hboxLayout13);

        machineSelectionTabs->addTab(zx81Tab, QString());
        spectrumTab = new QWidget();
        spectrumTab->setObjectName(QString::fromUtf8("spectrumTab"));
        vboxLayout14 = new QVBoxLayout(spectrumTab);
        vboxLayout14->setObjectName(QString::fromUtf8("vboxLayout14"));
        hboxLayout14 = new QHBoxLayout();
        hboxLayout14->setObjectName(QString::fromUtf8("hboxLayout14"));
        formLayout14 = new QFormLayout();
        formLayout14->setObjectName(QString::fromUtf8("formLayout14"));
        label24 = new QLabel(spectrumTab);
        label24->setObjectName(QString::fromUtf8("label24"));

        formLayout14->setWidget(0, QFormLayout::LabelRole, label24);

        spectrumModelComboBox = new QComboBox(spectrumTab);
        spectrumModelComboBox->addItem(QString());
        spectrumModelComboBox->addItem(QString());
        spectrumModelComboBox->addItem(QString());
        spectrumModelComboBox->addItem(QString());
        spectrumModelComboBox->addItem(QString());
        spectrumModelComboBox->addItem(QString());
        spectrumModelComboBox->setObjectName(QString::fromUtf8("spectrumModelComboBox"));

        formLayout14->setWidget(0, QFormLayout::FieldRole, spectrumModelComboBox);


        hboxLayout14->addLayout(formLayout14);

        spacerItem17 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hboxLayout14->addItem(spacerItem17);


        vboxLayout14->addLayout(hboxLayout14);

        machineSelectionTabs->addTab(spectrumTab, QString());

        verticalLayout->addWidget(machineSelectionTabs);

        topTipLabel = new QLabel(openGLWidget);
        topTipLabel->setObjectName(QString::fromUtf8("topTipLabel"));
        topTipLabel->setWordWrap(true);

        verticalLayout->addWidget(topTipLabel, 0, Qt::AlignBottom);

        startMachineButton = new QPushButton(openGLWidget);
        startMachineButton->setObjectName(QString::fromUtf8("startMachineButton"));

        verticalLayout->addWidget(startMachineButton, 0, Qt::AlignRight|Qt::AlignBottom);

        missingROMsBox = new QPlainTextEdit(openGLWidget);
        missingROMsBox->setObjectName(QString::fromUtf8("missingROMsBox"));
        missingROMsBox->setReadOnly(true);

        verticalLayout->addWidget(missingROMsBox);

        volumeSlider = new QSlider(openGLWidget);
        volumeSlider->setObjectName(QString::fromUtf8("volumeSlider"));
        volumeSlider->setMinimumSize(QSize(250, 0));
        volumeSlider->setOrientation(Qt::Horizontal);

        verticalLayout->addWidget(volumeSlider, 0, Qt::AlignHCenter|Qt::AlignBottom);

        MainWindow->setCentralWidget(openGLWidget);

        retranslateUi(MainWindow);

        machineSelectionTabs->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        actionDo_this->setText(QCoreApplication::translate("MainWindow", "Do this", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "Chip RAM:", nullptr));
        amigaChipRAMComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "512 kb", nullptr));
        amigaChipRAMComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "1 mb", nullptr));
        amigaChipRAMComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "2 mb", nullptr));

        label1->setText(QCoreApplication::translate("MainWindow", "Fast RAM:", nullptr));
        amigaFastRAMComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "None", nullptr));
        amigaFastRAMComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "1 mb", nullptr));
        amigaFastRAMComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "2 mb", nullptr));
        amigaFastRAMComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "4 mb", nullptr));
        amigaFastRAMComboBox->setItemText(4, QCoreApplication::translate("MainWindow", "8 mb", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(amigaTab), QCoreApplication::translate("MainWindow", "Amiga", nullptr));
        label2->setText(QCoreApplication::translate("MainWindow", "Model:", nullptr));
        appleIIModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Apple II", nullptr));
        appleIIModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Apple II+", nullptr));
        appleIIModelComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Apple IIe", nullptr));
        appleIIModelComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "Enhanced IIe", nullptr));

        label3->setText(QCoreApplication::translate("MainWindow", "Disk Controller:", nullptr));
        appleIIDiskControllerComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Sixteen Sector", nullptr));
        appleIIDiskControllerComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Thirteen Sector", nullptr));
        appleIIDiskControllerComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "None", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(appleIITab), QCoreApplication::translate("MainWindow", "Apple II", nullptr));
        label4->setText(QCoreApplication::translate("MainWindow", "Model:", nullptr));
        appleIIgsModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "ROM 00", nullptr));
        appleIIgsModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "ROM 01", nullptr));
        appleIIgsModelComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "ROM 03", nullptr));

        label5->setText(QCoreApplication::translate("MainWindow", "Memory Size:", nullptr));
        appleIIgsMemorySizeComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "256kb", nullptr));
        appleIIgsMemorySizeComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "1mb", nullptr));
        appleIIgsMemorySizeComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "8mb", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(appleIIgsTab), QCoreApplication::translate("MainWindow", "Apple IIgs", nullptr));
        label6->setText(QCoreApplication::translate("MainWindow", "Model:", nullptr));
        amstradCPCModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "CPC464", nullptr));
        amstradCPCModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "CPC664", nullptr));
        amstradCPCModelComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "CPC6128", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(amstradCPCTab), QCoreApplication::translate("MainWindow", "Amstrad CPC", nullptr));
        label7->setText(QCoreApplication::translate("MainWindow", "RAM:", nullptr));
        atariSTRAMComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "512 kb", nullptr));
        atariSTRAMComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "1 mb", nullptr));
        atariSTRAMComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "4 mb", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(atariSTTab), QCoreApplication::translate("MainWindow", "Atari ST", nullptr));
        electronDFSCheckBox->setText(QCoreApplication::translate("MainWindow", "With Disk Filing System", nullptr));
        electronADFSCheckBox->setText(QCoreApplication::translate("MainWindow", "With Advanced Disk Filing System", nullptr));
        electronAP6CheckBox->setText(QCoreApplication::translate("MainWindow", "With Advanced Plus 6 Utility ROM", nullptr));
        electronSidewaysRAMCheckBox->setText(QCoreApplication::translate("MainWindow", "Fill unused ROM banks with sideways RAM", nullptr));
        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(electronTab), QCoreApplication::translate("MainWindow", "Electron", nullptr));
        label8->setText(QCoreApplication::translate("MainWindow", "Model:", nullptr));
        enterpriseModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Enterprise 64", nullptr));
        enterpriseModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Enterprise 128", nullptr));
        enterpriseModelComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Enterprise 256", nullptr));

        label9->setText(QCoreApplication::translate("MainWindow", "Speed:", nullptr));
        enterpriseSpeedComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "4 MHz", nullptr));
        enterpriseSpeedComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "6 MHz", nullptr));

        label10->setText(QCoreApplication::translate("MainWindow", "EXOS:", nullptr));
        enterpriseEXOSComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Version 1.0", nullptr));
        enterpriseEXOSComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Version 2.0", nullptr));
        enterpriseEXOSComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Version 2.1", nullptr));

        label11->setText(QCoreApplication::translate("MainWindow", "BASIC:", nullptr));
        enterpriseBASICComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "None", nullptr));
        enterpriseBASICComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Version 1.0", nullptr));
        enterpriseBASICComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Version 1.1", nullptr));
        enterpriseBASICComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "Version 2.1", nullptr));

        label12->setText(QCoreApplication::translate("MainWindow", "DOS:", nullptr));
        enterpriseDOSComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "None", nullptr));
        enterpriseDOSComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "EXDOS", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(enterpriseTab), QCoreApplication::translate("MainWindow", "Enterprise", nullptr));
        label13->setText(QCoreApplication::translate("MainWindow", "Model:", nullptr));
        macintoshModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "128k", nullptr));
        macintoshModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "512k", nullptr));
        macintoshModelComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "512ke", nullptr));
        macintoshModelComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "Plus", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(macintoshTab), QCoreApplication::translate("MainWindow", "Macintosh", nullptr));
        label14->setText(QCoreApplication::translate("MainWindow", "Model", nullptr));
        msxModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "MSX 1", nullptr));
        msxModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "MSX 2", nullptr));

        label15->setText(QCoreApplication::translate("MainWindow", "Region", nullptr));
        msxRegionComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "European (PAL)", nullptr));
        msxRegionComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "American (NTSC)", nullptr));
        msxRegionComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Japanese (NTSC)", nullptr));

        msxDiskDriveCheckBox->setText(QCoreApplication::translate("MainWindow", "Attach disk drive", nullptr));
        msxMSXMUSICCheckBox->setText(QCoreApplication::translate("MainWindow", "Attach MSX-MUSIC", nullptr));
        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(msxTab), QCoreApplication::translate("MainWindow", "MSX", nullptr));
        label16->setText(QCoreApplication::translate("MainWindow", "Model:", nullptr));
        oricModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Oric-1", nullptr));
        oricModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Oric Atmos", nullptr));
        oricModelComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Pravetz 8D", nullptr));

        label17->setText(QCoreApplication::translate("MainWindow", "Disk Interface:", nullptr));
        oricDiskInterfaceComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "None", nullptr));
        oricDiskInterfaceComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Microdisc", nullptr));
        oricDiskInterfaceComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Jasmin", nullptr));
        oricDiskInterfaceComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "8DOS", nullptr));
        oricDiskInterfaceComboBox->setItemText(4, QCoreApplication::translate("MainWindow", "Byte Drive 500", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(oricTab), QCoreApplication::translate("MainWindow", "Oric", nullptr));
        label18->setText(QCoreApplication::translate("MainWindow", "Video Adaptor:", nullptr));
        pcVideoAdaptorComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "MDA", nullptr));
        pcVideoAdaptorComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "CGA", nullptr));

        label19->setText(QCoreApplication::translate("MainWindow", "Speed:", nullptr));
        pcSpeedComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Similar to Original", nullptr));
        pcSpeedComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "Turbo", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(pcTab), QCoreApplication::translate("MainWindow", "PC Compatible", nullptr));
        label20->setText(QCoreApplication::translate("MainWindow", "Region:", nullptr));
        vic20RegionComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "European (PAL)", nullptr));
        vic20RegionComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "American (NTSC)", nullptr));
        vic20RegionComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "Danish (PAL)", nullptr));
        vic20RegionComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "Swedish (PAL)", nullptr));
        vic20RegionComboBox->setItemText(4, QCoreApplication::translate("MainWindow", "Japanese (NTSC)", nullptr));

        label21->setText(QCoreApplication::translate("MainWindow", "Memory Size:", nullptr));
        vic20MemorySizeComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Unexpanded", nullptr));
        vic20MemorySizeComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "8 kb", nullptr));
        vic20MemorySizeComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "32 kb", nullptr));

        vic20C1540CheckBox->setText(QCoreApplication::translate("MainWindow", "Attach C-1540 disk drive", nullptr));
        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(vic20Tab), QCoreApplication::translate("MainWindow", "Vic-20", nullptr));
        label22->setText(QCoreApplication::translate("MainWindow", "Memory Size:", nullptr));
        zx80MemorySizeComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Unexpanded", nullptr));
        zx80MemorySizeComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "16 kb", nullptr));

        zx80UseZX81ROMCheckBox->setText(QCoreApplication::translate("MainWindow", "Use ZX81 ROM", nullptr));
        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(zx80Tab), QCoreApplication::translate("MainWindow", "ZX80", nullptr));
        label23->setText(QCoreApplication::translate("MainWindow", "Memory Size:", nullptr));
        zx81MemorySizeComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "Unexpanded", nullptr));
        zx81MemorySizeComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "16 kb", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(zx81Tab), QCoreApplication::translate("MainWindow", "ZX81", nullptr));
        label24->setText(QCoreApplication::translate("MainWindow", "Model:", nullptr));
        spectrumModelComboBox->setItemText(0, QCoreApplication::translate("MainWindow", "16kb", nullptr));
        spectrumModelComboBox->setItemText(1, QCoreApplication::translate("MainWindow", "48kb", nullptr));
        spectrumModelComboBox->setItemText(2, QCoreApplication::translate("MainWindow", "128kb", nullptr));
        spectrumModelComboBox->setItemText(3, QCoreApplication::translate("MainWindow", "+2", nullptr));
        spectrumModelComboBox->setItemText(4, QCoreApplication::translate("MainWindow", "+2a", nullptr));
        spectrumModelComboBox->setItemText(5, QCoreApplication::translate("MainWindow", "+3", nullptr));

        machineSelectionTabs->setTabText(machineSelectionTabs->indexOf(spectrumTab), QCoreApplication::translate("MainWindow", "ZX Spectrum", nullptr));
        topTipLabel->setText(QCoreApplication::translate("MainWindow", "TIP: the easiest way to get started is just to open the disk, tape or cartridge you want to use. The emulator will automatically configure a suitable machine and attempt to launch the software you've selected. Use this method to load Atari 2600, ColecoVision or Master System games.", nullptr));
        startMachineButton->setText(QCoreApplication::translate("MainWindow", "Start Machine", nullptr));
        missingROMsBox->setPlainText(QCoreApplication::translate("MainWindow", "Clock Signal requires you to provide images of the system ROMs for this machine. They will be stored permanently; you need do this only once.\n"
"\n"
"Please drag and drop over this window", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
