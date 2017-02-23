#! /usr/bin/python3
import dbus

class Connection:
    def __init__(self):
        self._conn = self._connect()

    def _connect(self):
        session_bus = dbus.SessionBus()

        pa_server_lookup = session_bus.get_object(
            'org.PulseAudio1',
            '/org/pulseaudio/server_lookup1')

        pa_server_address = pa_server_lookup.Get(
            'org.PulseAudio.ServerLookup1',
            'Address',
            dbus_interface='org.freedesktop.DBus.Properties')

        return dbus.connection.Connection(pa_server_address)

    def Core(self):
        return Core(self._conn, '/org/pulseaudio/core1')

class Object:
    def __init__(self, conn, path):
        self._conn = conn
        self._path = path
        self._obj = self._conn.get_object(object_path=self._path)

    def Path(self):
        return self._path

class Core(Object):
    def Extensions(self):
        return [str(path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'Extensions',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def Modules(self):
        return [Module(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'Modules',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def Cards(self):
        return [Card(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'Cards',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def Sources(self):
        return [Device(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'Sources',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def Sinks(self):
        return [Device(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'Sinks',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def Clients(self):
        return [Client(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'Clients',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def PlaybackStreams(self):
        return [Stream(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'PlaybackStreams',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def RecordStreams(self):
        return [Stream(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1',
            'PlaybackStreams',
            dbus_interface='org.freedesktop.DBus.Properties')]

class Module(Object):
    def Index(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.Module',
            'Index',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Name(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.Module',
            'Name',
            dbus_interface='org.freedesktop.DBus.Properties'))

class Card(Object):
    def Index(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'Index',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Name(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'Name',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Driver(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'Driver',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def OwnerModule(self):
        return Module(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'OwnerModule',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Sinks(self):
        return [Device(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'Sinks',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def Sources(self):
        return [Device(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'Sources',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def Profiles(self):
        return [CardProfile(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'Profiles',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def ActiveProfile(self):
        return CardProfile(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Card',
            'ActiveProfile',
            dbus_interface='org.freedesktop.DBus.Properties'))

class CardProfile(Object):
    def Index(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.CardProfile',
            'Index',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Name(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.CardProfile',
            'Name',
            dbus_interface='org.freedesktop.DBus.Properties'))

class Device(Object):
    def Index(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.Device',
            'Index',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Name(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.Device',
            'Name',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Driver(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.Device',
            'Driver',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def OwnerModule(self):
        return Module(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Device',
            'OwnerModule',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Ports(self):
        return [DevicePort(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1.Device',
            'Ports',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def ActivePort(self):
        return DevicePort(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Device',
            'ActivePort',
            dbus_interface='org.freedesktop.DBus.Properties'))

class DevicePort(Object):
    def Index(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.DevicePort',
            'Index',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Name(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.DevicePort',
            'Name',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Priority(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.DevicePort',
            'Priority',
            dbus_interface='org.freedesktop.DBus.Properties'))

class Client(Object):
    def Index(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.Client',
            'Index',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Driver(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.Client',
            'Driver',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def OwnerModule(self):
        return Module(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Client',
            'OwnerModule',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def PlaybackStreams(self):
        return [Stream(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1.Client',
            'PlaybackStreams',
            dbus_interface='org.freedesktop.DBus.Properties')]

    def RecordStreams(self):
        return [Stream(self._conn, path) for path in self._obj.Get(
            'org.PulseAudio.Core1.Client',
            'RecordStreams',
            dbus_interface='org.freedesktop.DBus.Properties')]

class Stream(Object):
    def Index(self):
        return int(self._obj.Get(
            'org.PulseAudio.Core1.Stream',
            'Index',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Driver(self):
        return str(self._obj.Get(
            'org.PulseAudio.Core1.Stream',
            'Driver',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def OwnerModule(self):
        return Module(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Stream',
            'OwnerModule',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Client(self):
        return Client(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Stream',
            'Client',
            dbus_interface='org.freedesktop.DBus.Properties'))

    def Device(self):
        return Device(self._conn, self._obj.Get(
            'org.PulseAudio.Core1.Stream',
            'Device',
            dbus_interface='org.freedesktop.DBus.Properties'))

def print_core(core):
    print(core.Path())

    print('  Extensions:')
    for extension in core.Extensions():
        print('    %s' % extension)
    print()

    print('  Modules:')
    for module in core.Modules():
        print('    %s' % module.Path())
        print('      Index: %s' % module.Index())
        print('      Name:  %s' % module.Name())
        print()

    print('  Cards:')
    for card in core.Cards():
        print('    %s' % card.Path())
        print('      Index:       %s' % card.Index())
        print('      Name:        %s' % card.Name())
        print('      Driver:      %s' % card.Driver())
        print('      OwnerModule: %s' % card.OwnerModule().Path())
        print('      Sinks:')
        for device in card.Sinks():
            print('        %s' % device.Path())
        print('      Sources:')
        for device in card.Sources():
            print('        %s' % device.Path())
        if card.Profiles():
            print('      Profiles:')
            for profile in card.Profiles():
                print('        %s' % profile.Path())
                print('          Index: %s' % profile.Index())
                print('          Name:  %s' % profile.Name())
            print('      ActiveProfile:')
            print('        %s' % card.ActiveProfile().Path())
        print()

    print('  Sinks:')
    for device in core.Sinks():
        print('    %s' % device.Path())
        print('      Index:       %s' % device.Index())
        print('      Name:        %s' % device.Name())
        print('      Driver:      %s' % device.Driver())
        print('      OwnerModule: %s' % device.OwnerModule().Path())
        if device.Ports():
            print('      Ports:')
            for port in device.Ports():
                print('        %s' % port.Path())
                print('          Index:    %s' % port.Index())
                print('          Name:     %s' % port.Name())
                print('          Priority: %s' % port.Priority())
            print('      ActivePort:')
            print('        %s' % device.ActivePort().Path())
        print()

    print('  Sources:')
    for device in core.Sources():
        print('    %s' % device.Path())
        print('      Index:       %s' % device.Index())
        print('      Name:        %s' % device.Name())
        print('      Driver:      %s' % device.Driver())
        print('      OwnerModule: %s' % device.OwnerModule().Path())
        if device.Ports():
            print('      Ports:')
            for port in device.Ports():
                print('        %s' % port.Path())
                print('          Index:    %s' % port.Index())
                print('          Name:     %s' % port.Name())
                print('          Priority: %s' % port.Priority())
            print('      ActivePort:')
            print('        %s' % device.ActivePort().Path())
        print()

    print('  Clients:')
    for client in core.Clients():
        print('    %s' % client.Path())
        print('      Index:       %s' % client.Index())
        print('      Driver:      %s' % client.Driver())
        print('      OwnerModule: %s' % client.OwnerModule().Path())
        print('      PlaybackStreams:')
        for stream in client.PlaybackStreams():
            print('        %s' % stream.Path())
        print('      RecordStreams:')
        for stream in client.RecordStreams():
            print('        %s' % stream.Path())
        print()

    print('  PlaybackStreams:')
    for stream in core.PlaybackStreams():
        print('    %s' % stream.Path())
        print('      Index:       %s' % stream.Index())
        print('      Driver:      %s' % stream.Driver())
        print('      OwnerModule: %s' % stream.OwnerModule().Path())
        print('      Stream:      %s' % stream.Client().Path())
        print('      Device:      %s' % stream.Device().Path())
        print()

    print('  RecordStreams:')
    for stream in core.RecordStreams():
        print('    %s' % stream.Path())
        print('      Index:       %s' % stream.Index())
        print('      Driver:      %s' % stream.Driver())
        print('      OwnerModule: %s' % stream.OwnerModule().Path())
        print('      Stream:      %s' % stream.Client().Path())
        print('      Device:      %s' % stream.Device().Path())
        print()

if __name__ == '__main__':
    conn = Connection()
    core = conn.Core()
    print_core(core)
