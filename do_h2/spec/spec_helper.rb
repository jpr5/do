$TESTING=true
JRUBY = true

require 'rubygems'
require 'rspec'
require 'date'
require 'ostruct'
require 'fileutils'

driver_lib = File.expand_path('../../lib', __FILE__)
$LOAD_PATH.unshift(driver_lib) unless $LOAD_PATH.include?(driver_lib)

# Prepend data_objects/do_jdbc in the repository to the load path.
# DO NOT USE installed gems, except when running the specs from gem.
repo_root = File.expand_path('../../..', __FILE__)
['data_objects', 'do_jdbc'].each do |lib|
  lib_path = "#{repo_root}/#{lib}/lib"
  $LOAD_PATH.unshift(lib_path) if File.directory?(lib_path) && !$LOAD_PATH.include?(lib_path)
end

require 'data_objects'
require 'data_objects/spec/setup'
require 'data_objects/spec/lib/pending_helpers'
require 'do_h2'

DataObjects::H2.logger = DataObjects::Logger.new(STDOUT, :off)
at_exit { DataObjects.logger.flush }


CONFIG              = OpenStruct.new
CONFIG.uri          = ENV["DO_H2_SPEC_URI"] || "jdbc:h2:mem"
CONFIG.driver       = 'h2'
CONFIG.jdbc_driver  = DataObjects::H2::JDBC_DRIVER

module DataObjectsSpecHelpers

  def setup_test_environment
    conn = DataObjects::Connection.new(CONFIG.uri)

    conn.create_command(<<-EOF).execute_non_query
      DROP TABLE IF EXISTS invoices
    EOF

    conn.create_command(<<-EOF).execute_non_query
      DROP TABLE IF EXISTS users
    EOF

    conn.create_command(<<-EOF).execute_non_query
      DROP TABLE IF EXISTS widgets
    EOF

    conn.create_command(<<-EOF).execute_non_query
      CREATE TABLE users (
        id                INTEGER IDENTITY,
        name              VARCHAR(200) default 'Billy' NULL,
        fired_at          TIMESTAMP
      )
    EOF

    conn.create_command(<<-EOF).execute_non_query
      CREATE TABLE invoices (
        id                INTEGER IDENTITY,
        invoice_number    VARCHAR(50) NOT NULL
      )
    EOF

    conn.create_command(<<-EOF).execute_non_query
      CREATE TABLE widgets (
        id                INTEGER IDENTITY,
        code              CHAR(8) DEFAULT 'A14' NULL,
        name              VARCHAR(200) DEFAULT 'Super Widget' NULL,
        shelf_location    VARCHAR NULL,
        description       LONGVARCHAR NULL,
        image_data        VARBINARY NULL,
        ad_description    LONGVARCHAR NULL,
        ad_image          VARBINARY NULL,
        whitepaper_text   LONGVARCHAR NULL,
        cad_drawing       LONGVARBINARY NULL,
        flags             BOOLEAN DEFAULT 0,
        number_in_stock   SMALLINT DEFAULT 500,
        number_sold       INTEGER DEFAULT 0,
        super_number      BIGINT DEFAULT 9223372036854775807,
        weight            FLOAT DEFAULT 1.23,
        cost1             REAL DEFAULT 10.23,
        cost2             DECIMAL DEFAULT 50.23,
        release_date      DATE DEFAULT '2008-02-14',
        release_datetime  DATETIME DEFAULT '2008-02-14 00:31:12',
        release_timestamp TIMESTAMP DEFAULT '2008-02-14 00:31:31'
      )
    EOF
    # XXX: H2 has no ENUM
    # status` enum('active','out of stock') NOT NULL default 'active'

    1.upto(16) do |n|
      conn.create_command(<<-EOF).execute_non_query
        INSERT INTO widgets(
          code,
          name,
          shelf_location,
          description,
          image_data,
          ad_description,
          ad_image,
          whitepaper_text,
          cad_drawing,
          super_number,
          weight)
        VALUES (
          'W#{n.to_s.rjust(7,"0")}',
          'Widget #{n}',
          'A14',
          'This is a description',
          '4f3d4331434343434331',
          'Buy this product now!',
          '4f3d4331434343434331',
          'String',
          '4f3d4331434343434331',
          1234,
          13.4);
      EOF

      ## TODO: change the hexadecimal examples

      conn.create_command(<<-EOF).execute_non_query
        update widgets set flags = true where id = 2
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set ad_description = NULL where id = 3
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set flags = NULL where id = 4
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set cost1 = NULL where id = 5
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set cost2 = NULL where id = 6
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set release_date = NULL where id = 7
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set release_datetime = NULL where id = 8
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set release_timestamp = NULL where id = 9
      EOF

      conn.create_command(<<-EOF).execute_non_query
        update widgets set release_datetime = '2008-07-14 00:31:12' where id = 10
      EOF

      conn.close
    end

  end
end

RSpec.configure do |config|
  config.include(DataObjectsSpecHelpers)
  config.include(DataObjects::Spec::PendingHelpers)
end
