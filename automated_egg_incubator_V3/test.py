import ConfigParser
import datetime


if __name__ == '__main__':
    config = ConfigParser.ConfigParser()
    config.readfp(open(r'config.conf'))
    incubation_date = datetime.datetime.strptime(config.get('Params', 'incubation_date'), "%d/%m/%y")
    birth_date = incubation_date + datetime.timedelta(days=21)
    dt = datetime.datetime
    now = dt.now()
    # This gives timedelta in days
    print(str(birth_date - dt(year=now.year, month=now.month, day=now.day)))
