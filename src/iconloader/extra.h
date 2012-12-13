#include <QIcon>

class Extra
{
public:
    static QIcon getIcon(const QString & iconName)
    {
        return QIcon(iconName);
    }
};
