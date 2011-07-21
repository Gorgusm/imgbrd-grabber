#include "searchtab.h"
#include "ui_searchtab.h"
#include "QBouton.h"
#include "zoomwindow.h"

#define DONE()	logUpdate(tr(" Fait"))
extern mainWindow *_mainwindow;



searchTab::searchTab(QMap<QString,QMap<QString,QString> > *sites, QMap<QString,QString> *favorites, QDateTime *serverDate, QWidget *parent) : QWidget(parent), ui(new Ui::searchTab), m_serverDate(serverDate), m_favorites(favorites), m_sites(sites), m_pagemax(-1), m_sized(false)
{
    ui->setupUi(this);
	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);

	// Search field
	m_search = new TextEdit(m_favorites->keys(), this);
	m_postFiltering = new TextEdit(m_favorites->keys(), this);
		m_search->setContextMenuPolicy(Qt::CustomContextMenu);
		m_postFiltering->setContextMenuPolicy(Qt::CustomContextMenu);
		QStringList completion;
			QFile words("words.txt");
			if (words.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				while (!words.atEnd())
				{
					QByteArray line = words.readLine();
					completion.append(QString(line).remove("\r\n").remove("\n").split(" ", QString::SkipEmptyParts));
				}
				completion.append(m_favorites->keys());
				completion.removeDuplicates();
				completion.sort();
				QCompleter *completer = new QCompleter(completion, this);
					completer->setCaseSensitivity(Qt::CaseInsensitive);
				m_search->setCompleter(completer);
				m_postFiltering->setCompleter(completer);
			}
		connect(m_search, SIGNAL(returnPressed()), this, SLOT(load()));
		connect(m_postFiltering, SIGNAL(returnPressed()), this, SLOT(load()));
		ui->layoutFields->insertWidget(1, m_search, 1);
		ui->layoutPlus->addWidget(m_postFiltering, 1, 1, 1, 3);

	// Calendar
	m_calendar = new QCalendarWidget;
		m_calendar->setLocale(QLocale(settings->value("language", "English").toString().toLower().left(2)));
		m_calendar->setWindowIcon(QIcon(":/images/icon.ico"));
		m_calendar->setWindowTitle("Grabber - Choisir une date");
		m_calendar->setDateRange(QDate(2000, 1, 1), m_serverDate->date());
		m_calendar->setSelectedDate(m_serverDate->date());
		connect(m_calendar, SIGNAL(activated(QDate)), this, SLOT(insertDate(QDate)));
		connect(m_calendar, SIGNAL(activated(QDate)), m_calendar, SLOT(close()));
	connect(ui->buttonCalendar, SIGNAL(clicked()), m_calendar, SLOT(show()));

	// Sources
	QString sel = '1'+QString().fill('0',m_sites->count()-1);
	QString sav = settings->value("sites", sel).toString();
	for (int i = 0; i < sel.count(); i++)
	{
		if (sav.count() <= i)
		{ sav[i] = '0'; }
		m_selectedSources.append(sav.at(i) == '1' ? true : false);
	}

	// Others
	ui->checkMergeResults->setChecked(settings->value("mergeresults", false).toBool());
	optionsChanged(settings);
	ui->widgetPlus->hide();
	setWindowIcon(QIcon());
	QPalette pal = palette();
		pal.setColor(backgroundRole(), QColor(255,255,255));
		setPalette(pal);
		setAutoFillBackground(true);
	updateCheckboxes();
	m_search->setFocus();

	// Splitter size
	QString sizes = settings->value("splitter").toString();
	if (!sizes.isEmpty())
	{
		QStringList sz = sizes.split("|");
		ui->splitter->setSizes(QList<int>() << sz[0].toInt() << sz[1].toInt());
		m_sized = true;
	}
}

searchTab::~searchTab()
{
	close();
	delete ui;
}

void searchTab::closeEvent(QCloseEvent *e)
{
	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);
	QList<int> sizes = ui->splitter->sizes();
	settings->setValue("splitter", QString::number(sizes[0])+"|"+QString::number(sizes[1]));
	settings->setValue("mergeresults", ui->checkMergeResults->isChecked());
	e->accept();
}



void searchTab::optionsChanged(QSettings *settings)
{
	ui->retranslateUi(this);
	log(tr("Mise � jour des options de l'onglet \"%1\".").arg(windowTitle()));
	ui->layoutResults->setHorizontalSpacing(settings->value("Margins/horizontal", 6).toInt());
	ui->layoutResults->setVerticalSpacing(settings->value("Margins/vertical", 6).toInt());
	ui->spinImagesPerPage->setValue(settings->value("limit", 20).toInt());
	ui->spinColumns->setValue(settings->value("columns", 1).toInt());
}



void searchTab::insertDate(QDate date)
{
	m_search->setText(m_search->toPlainText()+" date:"+date.toString("MM/dd/yyyy"));
	m_search->doColor();
}



void searchTab::openSourcesWindow()
{
	sourcesWindow *adv = new sourcesWindow(m_selectedSources, m_sites, this);
	connect(adv, SIGNAL(valid(QList<bool>)), this, SLOT(saveSources(QList<bool>)));
	adv->show();
}
void searchTab::saveSources(QList<bool> sel)
{
	log(tr("Sauvegarde des sources..."));
	m_selectedSources = sel;
	QString sav;
	for (int i = 0; i < m_selectedSources.count(); i++)
	{ sav += (m_selectedSources.at(i) ? "1" : "0"); }
	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);
	settings->setValue("sites", sav);
	DONE();
	updateCheckboxes();
}
void searchTab::updateCheckboxes()
{
	log(tr("Mise � jour des cases � cocher."));
	for (int i = 0; i < m_checkboxes.size(); i++)
	{ delete m_checkboxes.at(i); }
	m_checkboxes.clear();
	QStringList urls = m_sites->keys();
	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);
	int n = settings->value("Sources/Letters", 3).toInt(), m = n;
	for (int i = 0; i < urls.size(); i++)
	{
		if (n < 0)
		{
			m = urls.at(i).indexOf('.');
			if (n < -1 && urls.at(i).indexOf('.', m+1) != -1)
			{ m = urls.at(i).indexOf('.', m+1); }
		}
		QCheckBox *c = new QCheckBox(urls.at(i).left(m));
			c->setChecked(m_selectedSources.at(i));
			ui->layoutSourcesList->addWidget(c);
		m_checkboxes.append(c);
	}
	DONE();
}



void searchTab::load()
{
	while (ui->layoutResults->count() > 0)
	{ ui->layoutResults->takeAt(0)->widget()->hide(); }
	setWindowTitle(m_search->toPlainText().isEmpty() ? tr("Recherche") : m_search->toPlainText());
	emit titleChanged(this);
	ui->labelTags->setText("");
	m_pages.clear();
	m_images.clear();
	for (int i = 0; i < m_selectedSources.count(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
		{
			Page *page = new Page(m_sites, m_sites->keys().at(i), m_search->toPlainText().toLower().split(" "), ui->spinPage->value(), ui->spinImagesPerPage->value(), m_postFiltering->toPlainText().toLower().split(" "));
			log(tr("Chargement de la page <a href=\"%1\">%1</a>").arg(page->url().toString()));
			connect(page, SIGNAL(finishedLoading(Page*)), this, SLOT(finishedLoading(Page*)));
			page->load();
			m_pages.append(page);
		}
	}
}

bool sortByFrequency(Tag *s1, Tag *s2)
{ return s1->count() > s2->count(); }
void searchTab::finishedLoading(Page* page)
{
	log(tr("R�ception de la page <a href=\"%1\">%1</a>").arg(page->url().toString()));
	if (page->imagesCount() < m_pagemax || m_pagemax == -1 )
	{ m_pagemax = page->imagesCount(); }
	QList<Image*> imgs = page->images();
	int pos = m_pages.indexOf(page);
	int pl = ceil(sqrt(ui->spinImagesPerPage->value()));
	float fl = (float)ui->spinImagesPerPage->value()/pl;
	QLabel *txt = new QLabel();
		if (imgs.count() == 0)
		{
			QStringList reasons = QStringList();
			if (page->source().isEmpty())
			{ reasons.append(tr("serveur hors-ligne")); }
			if (m_search->toPlainText().count(" ") > 1)
			{ reasons.append(tr("trop de tags")); }
			if (ui->spinPage->value() > 1000)
			{ reasons.append(tr("page trop �loign�e")); }
			txt->setText(m_sites->key(page->site())+" - <a href=\""+page->url().toString()+"\">"+page->url().toString()+"</a> - "+tr("Aucun r�sultat")+(reasons.count() > 0 ? "<br/>"+tr("Raisons possibles : %1").arg(reasons.join(", ")) : ""));
		}
		else
		{ txt->setText(m_sites->key(page->site())+" - <a href=\""+page->url().toString()+"\">"+page->url().toString()+"</a> - "+tr("Page %1 sur %2 (%3 sur %4)").arg(ui->spinPage->value()).arg(page->imagesCount() != 0 ? ceil(page->imagesCount()/((float)ui->spinImagesPerPage->value())) : 0).arg(imgs.count()).arg(page->imagesCount() != 0 ? page->imagesCount() : 0)); }
		txt->setOpenExternalLinks(true);
	ui->layoutResults->addWidget(txt, floor(pos/ui->spinColumns->value())*(fl+1), pl*(pos%ui->spinColumns->value()), 1, pl);
	ui->layoutResults->setRowMinimumHeight((floor(pos/ui->spinColumns->value())*(fl+1)), 50);
	m_images.append(imgs);

	// Tags for this page
	QList<Tag*> taglist;
	QStringList tagsGot;
	for (int i = 0; i < m_pages.count(); i++)
	{
		QList<Tag*> tags = m_pages.at(i)->tags();
		for (int t = 0; t < tags.count(); t++)
		{
			if (tagsGot.contains(tags.at(t)->text()))
			{ taglist[tagsGot.indexOf(tags.at(t)->text())]->setCount(taglist[tagsGot.indexOf(tags.at(t)->text())]->count()+tags.at(t)->count()); }
			else
			{
				taglist.append(tags.at(t));
				tagsGot.append(tags.at(t)->text());
			}
		}
	}

	// We sort tags by frequency
	qSort(taglist.begin(), taglist.end(), sortByFrequency);

	// Then we show them, styled if possible
	QStringList tlist = QStringList() << "artists" << "copyrights" << "characters" << "models" << "generals" << "favorites" << "blacklisteds";
	QStringList defaults = QStringList() << "#aa0000" << "#aa00aa" << "#00aa00" << "#0000ee" << "#000000" << "#ffc0cb" << "#000000";
	QMap<QString,QString> styles;
	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);
	for (int i = 0; i < tlist.size(); i++)
	{
		QFont font;
		font.fromString(settings->value("Coloring/Fonts/"+tlist.at(i)).toString());
		styles[tlist.at(i)] = "color:"+settings->value("Coloring/Colors/"+tlist.at(i), defaults.at(i)).toString()+"; "+qfonttocss(font);
	}
	QString tags;
	for (int i = 0; i < taglist.count(); i++)
	{
		if (m_favorites->contains(taglist[i]->text()))
		{ taglist[i]->setType("favorite"); }
		tags += "<a href=\""+taglist[i]->text()+"\" style=\""+(styles.contains(taglist[i]->type()+"s") ? styles[taglist[i]->type()+"s"] : styles["generals"])+"\">"+taglist[i]->text()+"</a> ("+QString::number(taglist[i]->count())+")<br/>";
	}
	ui->labelTags->setText(tags);
	if (!m_sized)
	{
		int s1 = ui->labelTags->sizeHint().width();
		if (s1 > width()*.2)
		{ s1 = width()*.2; }
		ui->splitter->setSizes(QList<int>() << s1 << width()-s1);
	}

	// Loading images
	for (int i = 0; i < imgs.count(); i++)
	{
		QStringList detected;
		Image *img = imgs.at(i);
		if (!settings->value("blacklistedtags").toString().isEmpty())
		{
			QStringList blacklistedtags(settings->value("blacklistedtags").toString().split(" "));
			for (int b = 0; b < blacklistedtags.size(); b++)
			{
				for (int t = 0; t < img->tags().count(); t++)
				{
					if (img->tags().at(t)->text().toLower() == blacklistedtags.at(b).toLower())
					{ detected.append(blacklistedtags.at(b)); }
				}
			}
		}
		if (!detected.isEmpty() && settings->value("hideblacklisted", false).toBool())
		{ log(tr("Image #%1 ignored. Reason: %2.").arg(i).arg("\""+detected.join(", ")+"\""));; }
		else
		{
			connect(img, SIGNAL(finishedLoadingPreview(Image*)), this, SLOT(finishedLoadingPreview(Image*)));
			img->loadPreview();
		}
	}
}

void searchTab::finishedLoadingPreview(Image *img)
{
	int position = m_images.indexOf(img);
	int page = m_pages.indexOf(img->page());
	QPixmap preview = img->previewImage();
	if (preview.isNull())
	{
		log("<b>Warning:</b> one of the preview pictures (<a href='"+img->previewUrl().toString()+"'>"+img->previewUrl().toString()+"</a>) is empty.");
		return;
	}

	QString unit;
	int size = img->fileSize();
	if (size >= 2048)
	{
		size /= 1024;
		if (size >= 2048)
		{
			size /= 1024;
			unit = "mo";
		}
		else
		{ unit = "ko"; }
	}
	else
	{ unit = "o"; }

	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);
	QColor color;
	if (img->status() == "pending")
	{ color = QColor("#0000ff"); }
	QBouton *l = new QBouton(position, this, settings->value("resizeInsteadOfCropping", true).toBool(), color);
		l->setIcon(preview);
		QString t;
		for (int i = 0; i < img->tags().count(); i++)
		{ t += " "+img->tags().at(i)->text(); }
		l->setToolTip(QString("%1%2%3%4%5%6%7%8")
			.arg(img->tags().isEmpty() ? " " : tr("<b>Tags :</b> %1<br/><br/>").arg(t.trimmed()))
			.arg(img->id() == 0 ? " " : tr("<b>ID :</b> %1<br/>").arg(img->id()))
			.arg(img->rating().isEmpty() ? " " : tr("<b>Classe :</b> %1<br/>").arg(img->rating()))
			.arg(tr("<b>Score :</b> %1<br/>").arg(img->score()))
			.arg(img->author().isEmpty() ? " " : tr("<b>Posteur :</b> %1<br/><br/>").arg(img->author()))
			.arg(img->width() == 0 || img->height() == 0 ? " " : tr("<b>Dimensions :</b> %1 x %2<br/>").arg(QString::number(img->width()), QString::number(img->height())))
			.arg(img->fileSize() == 0 ? " " : tr("<b>Taille :</b> %1 %2<br/>").arg(QString::number(round(size)), unit))
			.arg(!img->createdAt().isValid() ? " " : tr("<b>Date :</b> %1").arg(img->createdAt().toString(tr("le dd/MM/yyyy � hh:mm"))))
		);
		l->setIconSize(preview.size());
		l->setFlat(true);
		connect(l, SIGNAL(appui(int)), this, SLOT(webZoom(int)));
		//connect(l, SIGNAL(rightClick(int)), this, SLOT(batchChange(int)));
	int pl = ceil(sqrt(ui->spinImagesPerPage->value()));
	float fl = (float)ui->spinImagesPerPage->value()/pl;
	ui->layoutResults->addWidget(l, floor(float(position%ui->spinImagesPerPage->value())/fl)+(floor(page/ui->spinColumns->value())*(fl+1))+1, (page%ui->spinColumns->value())*pl+position%pl, 1, 1);
}

void searchTab::webZoom(int id)
{
	QStringList detected;
	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);
	Image *image = m_images.at(id);
	if (!settings->value("blacklistedtags").toString().isEmpty())
	{
		QStringList blacklistedtags(settings->value("blacklistedtags").toString().split(" "));
		for (int i = 0; i < blacklistedtags.size(); i++)
		{
			for (int t = 0; t < image->tags().count(); t++)
			{
				if (image->tags().at(t)->text().toLower() == blacklistedtags.at(i).toLower())
				{ detected.append(blacklistedtags.at(i)); }
			}
		}
		if (!detected.isEmpty())
		{
			int reply = QMessageBox::question(this, tr("List noire"), tr("%n tag(s) figurant dans la liste noire d�t�ct�(s) sur cette image : %1. Voulez-vous l'afficher tout de m�me ?", "", detected.size()).arg(detected.join(", ")), QMessageBox::Yes | QMessageBox::No);
			if (reply == QMessageBox::No)
			{ return; }
		}
	}
	zoomWindow *zoom = new zoomWindow(image, image->page()->site());
	zoom->show();
	connect(zoom, SIGNAL(linkClicked(QString)), this, SLOT(setTags(QString)));
}

void searchTab::setTags(QString tags)
{
	activateWindow();
	m_search->setText(tags);
	m_search->doColor();
	load();
}

void searchTab::getPage()
{
	QStringList actuals, keys = m_sites->keys();
	for (int i = 0; i < m_checkboxes.count(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
		{ actuals.append(keys.at(i)); }
	}
	QSettings *settings = new QSettings(savePath("settings.ini"), QSettings::IniFormat);
	for (int i = 0; i < actuals.count(); i++)
	{ emit batchAddGroup(QStringList() << m_search->toPlainText() << QString::number(ui->spinPage->value()) << QString::number(ui->spinImagesPerPage->value()) << QString::number(ui->spinImagesPerPage->value()) << "false" << actuals.at(i) << settings->value("Save/filename").toString() << settings->value("Save/path").toString() << ""); }
}

void searchTab::firstPage()
{
	ui->spinPage->setValue(1);
	load();
}
void searchTab::previousPage()
{
	if (ui->spinPage->value() > 1)
	{
		ui->spinPage->setValue(ui->spinPage->value()-1);
		load();
	}
}
void searchTab::nextPage()
{
	if (ui->spinPage->value() < m_pagemax)
	{
		ui->spinPage->setValue(ui->spinPage->value()+1);
		load();
	}
}
void searchTab::lastPage()
{
	ui->spinPage->setValue(m_pagemax);
	load();
}



void searchTab::linkHovered(QString url)
{ m_link = url; }
void searchTab::linkClicked(QString url)
{
	m_search->setPlainText(url);
	load();
}
void searchTab::contextMenu()
{
	QMenu *menu = new QMenu(this);
	if (!this->m_link.isEmpty())
	{
		if (m_favorites->contains(m_link))
		{ menu->addAction(QIcon(":/images/icons/remove.png"), tr("Retirer des favoris"), this, SLOT(unfavorite())); }
		else
		{ menu->addAction(QIcon(":/images/icons/add.png"), tr("Ajouter aux favoris"), this, SLOT(favorite())); }
		QStringList vil = loadViewItLater();
		if (vil.contains(m_link, Qt::CaseInsensitive))
		{ menu->addAction(QIcon(":/images/icons/remove.png"), tr("Ne pas garder pour plus tard"), this, SLOT(unviewitlater())); }
		else
		{ menu->addAction(QIcon(":/images/icons/add.png"), tr("Garder pour plus tard"), this, SLOT(viewitlater())); }
		menu->addAction(QIcon(":/images/icons/window.png"), tr("Ouvrir dans une nouvelle fen�tre"), this, SLOT(openInNewWindow()));
	}
	menu->exec(QCursor::pos());
}
void searchTab::openInNewWindow()
{
	QProcess myProcess;
	myProcess.startDetached(qApp->arguments().at(0), QStringList(m_link));
}
void searchTab::favorite()
{
	QString v = "50|"+QDateTime::currentDateTime().toString(Qt::ISODate);
	m_favorites->insert(m_link, v);
	QFile f(savePath("favorites.txt"));
		f.open(QIODevice::WriteOnly | QIODevice::Append);
		f.write(QString(m_link+"|"+v+"\r\n").toAscii());
	f.close();
	/*QPixmap img = image;
	if (img.width() > 150 || img.height() > 150)
	{ img = img.scaled(QSize(150,150), Qt::KeepAspectRatio, Qt::SmoothTransformation); }
	if (!QDir(savePath("thumbs")).exists())
	{ QDir(savePath()).mkdir("thumbs"); }
	img.save(savePath("thumbs/"+m_link+".png"), "PNG");*/
	_mainwindow->updateFavorites();
}
void searchTab::unfavorite()
{
	m_favorites->remove(m_link);
	QFile f(savePath("favorites.txt"));
	f.open(QIODevice::ReadOnly);
		QString favs = f.readAll();
	f.close();
	favs.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n");
	QRegExp reg(m_link+"\\|(.+)\\r\\n");
	reg.setMinimal(true);
	favs.remove(reg);
	f.open(QIODevice::WriteOnly);
		f.write(favs.toAscii());
	f.close();
	if (QFile::exists(savePath("thumbs/"+m_link+".png")))
	{ QFile::remove(savePath("thumbs/"+m_link+".png")); }
	_mainwindow->updateFavorites();
}
void searchTab::viewitlater()
{
	QStringList vil = loadViewItLater();
	vil.append(m_link);
	QFile f(savePath("viewitlater.txt"));
	f.open(QIODevice::WriteOnly);
		f.write(vil.join("\r\n").toAscii());
	f.close();
}
void searchTab::unviewitlater()
{
	QStringList vil = loadViewItLater();
	vil.removeAll(m_link);
	QFile f(savePath("viewitlater.txt"));
	f.open(QIODevice::WriteOnly);
		f.write(vil.join("\r\n").toAscii());
	f.close();
}
