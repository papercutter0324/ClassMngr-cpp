#include <QCoreApplication>
#include <QPdfDocument>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QPdfDocument candidate;
    QPdfDocument reference;
    QTextStream out(stdout);
    if (argc != 3
        || candidate.load(QString::fromLocal8Bit(argv[1])) != QPdfDocument::Error::None
        || reference.load(QString::fromLocal8Bit(argv[2])) != QPdfDocument::Error::None)
        return 2;
    if (candidate.pageCount() != reference.pageCount())
    {
        out << "page_count_mismatch candidate=" << candidate.pageCount()
            << " reference=" << reference.pageCount() << '\n';
        return 3;
    }
    int matchingPages = 0;
    for (int page = 0; page < candidate.pageCount(); ++page)
    {
        const QString generated = candidate.getAllText(page).text().simplified();
        const QString baseline = reference.getAllText(page).text().simplified();
        if (candidate.pagePointSize(page) != reference.pagePointSize(page))
        {
            out << "page=" << page + 1 << " dimension_mismatch\n";
            return 4;
        }
        if (generated != baseline)
        {
            qsizetype i = 0;
            const qsizetype shared = qMin(generated.size(), baseline.size());
            while (i < shared && generated.at(i) == baseline.at(i)) ++i;
            out << "page=" << page + 1 << " text_mismatch_at=" << i
                << " generated_length=" << generated.size()
                << " reference_length=" << baseline.size() << '\n';
        }
        else
        {
            ++matchingPages;
        }
    }
    out << "pages=" << candidate.pageCount()
        << " exact_simplified_text_matches=" << matchingPages
        << " differing_pages=" << candidate.pageCount() - matchingPages
        << " custom_character_mapping=" << "none" << '\n';
    return matchingPages == candidate.pageCount() ? 0 : 5;
}
