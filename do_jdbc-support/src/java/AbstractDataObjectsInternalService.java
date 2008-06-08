
import data_objects.Command;
import data_objects.Connection;
import data_objects.Reader;
import data_objects.Result;
import data_objects.Transaction;
import data_objects.drivers.DriverDefinition;
import java.io.IOException;
import org.jruby.Ruby;
import org.jruby.RubyClass;
import org.jruby.RubyModule;
import org.jruby.runtime.load.BasicLibraryService;

import static data_objects.DataObjects.DATA_OBJECTS_MODULE_NAME;

/**
 * AbstractDataObjectsInternalService
 *
 * @author alexbcoles
 */
public abstract class AbstractDataObjectsInternalService implements BasicLibraryService {

    public boolean basicLoad(final Ruby runtime) throws IOException {

        final String moduleName = getModuleName();
        final String errorName = getErrorName();
        final DriverDefinition driverDefinition = getDriverDefinition();

        // Get the DataObjects module
        RubyModule doModule = runtime.getModule(DATA_OBJECTS_MODULE_NAME);

        // Define the DataObjects module for this Driver
        // e.g. DataObjects::Derby, DataObjects::MySql
        RubyModule doDriverModule = doModule.defineModuleUnder(errorName);

        // Define a driver Error class
        runtime.defineClass(getErrorName(), runtime.getStandardError(), runtime.getStandardError().getAllocator());

        // Define the DataObjects driver classes
        RubyClass command = Command.createCommandClass(runtime, moduleName, errorName, driverDefinition);
        RubyClass connection = Connection.createConnectionClass(runtime, moduleName, errorName, driverDefinition);
        RubyClass result = Result.createResultClass(runtime, moduleName, errorName, driverDefinition);
        RubyClass reader = Reader.createReaderClass(runtime, moduleName, errorName, driverDefinition);
        RubyClass transaction = Transaction.createTransactionClass(runtime, moduleName, errorName, driverDefinition);

        return true;
    }

    public abstract String getModuleName();

    public String getErrorName() {
       return getModuleName() + "Error";
    }

    public abstract DriverDefinition getDriverDefinition();

}