
##################################################################################
# Data sources
##################################################################################

multisite_datasources["hosts"] = {
    "title"   : "All hosts",
    "table"   : "hosts",
    "columns" : ["host_name", "host_alias", "host_state", "host_has_been_checked", "host_downtimes", 
		 "host_plugin_output", "host_num_services", "host_num_services_pending", "host_num_services_ok", 
		 "host_num_services_warn", 
   "host_num_services_unknown", "host_num_services_crit" ],
}

multisite_datasources["services"] = {
    "title"   : "All services",
    "table"   : "services",
    "columns" : ["service_description", "service_plugin_output", "service_state", "service_has_been_checked", 
                 "host_name", "host_state", "host_has_been_checked", 
		 "service_last_state_change", "service_downtimes", "service_perf_data" ],
}

multisite_datasources["servicesbygroup"] = {
    "title"   : "Service grouped by service groups",
    "table"   : "servicesbygroup",
    "columns" : ["servicegroup_alias", "servicegroup_name", 
		 "service_description", "service_plugin_output", "service_state", "service_has_been_checked", 
                 "host_name", "host_state", "host_has_been_checked", 
		 "service_last_state_change", "service_downtimes", "service_perf_data" ],
}

multisite_datasources["servicegroups"] = {
    "title" : "Servicegroups",
    "table" : "servicegroups",
    "columns" : \
	[ "servicegroup_name", "servicegroup_alias", "servicegroup_num_services", 
	  "servicegroup_num_services_ok", "servicegroup_num_services_warn", 
	  "servicegroup_num_services_crit", "servicegroup_num_services_unknown", 
	  "servicegroup_num_services_pending", "servicegroup_worst_service_state" ],
}

multisite_datasources["hostgroups"] = {
    "title" : "Hostgroups",
    "table" : "hostgroups",
    "columns" :
       	[ "hostgroup_name", "hostgroup_alias", "hostgroup_num_hosts", "hostgroup_num_hosts_up", 
	  "hostgroup_num_hosts_down", "hostgroup_num_hosts_pending", "hostgroup_num_hosts_unreach",
	  "hostgroup_num_services", "hostgroup_num_services_ok", "hostgroup_num_services_warn", 
	  "hostgroup_num_services_crit", "hostgroup_num_services_unknown", "hostgroup_num_services_pending",
	  "hostgroup_worst_service_state" ],
}

