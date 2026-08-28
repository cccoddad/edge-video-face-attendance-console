#include "qquerywidget.h"
#include "ui_qquerywidget.h"


QqueryWidget::QqueryWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QqueryWidget)
{
    ui->setupUi(this);
    //往下拉列表中添加数据
    ui->QtableCbb->addItem("员工数据");
    ui->QtableCbb->addItem("考勤数据");

    //初始化表格模型
    model = new QSqlTableModel();
    //默认绑定到用户表格
    model->setTable("user");
    //把数据模型与tableview绑定
    ui->QdataView->setModel(model);
}

QqueryWidget::~QqueryWidget()
{
    delete ui;
}

void QqueryWidget::on_QqueryBt_clicked()
{
    QString tablename[2]={"user","recorduser"};
    //获取下拉列表当前索引号
    int index = ui->QtableCbb->currentIndex();
    if(index <0)index=0;
    //设置模型对应的表格
    model->setTable(tablename[index]);
    //查询数据
    model->select();
}
