// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_SITE_DATA_SIZE_COLLECTOR_H_
#define CHROME_BROWSER_BROWSING_DATA_SITE_DATA_SIZE_COLLECTOR_H_

#include <list>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/database_helper.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "components/browsing_data/content/indexed_db_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/browsing_data/content/service_worker_helper.h"
#include "content/public/browser/storage_partition.h"

class SiteDataSizeCollector {
 public:
  using CookieList = std::list<net::CanonicalCookie>;
  using DatabaseInfoList = std::list<content::StorageUsageInfo>;
  using LocalStorageInfoList = std::list<content::StorageUsageInfo>;
  using IndexedDBInfoList = std::list<content::StorageUsageInfo>;
  using FileSystemInfoList =
      std::list<browsing_data::FileSystemHelper::FileSystemInfo>;
  using ServiceWorkerUsageInfoList = std::list<content::StorageUsageInfo>;
  using CacheStorageUsageInfoList = std::list<content::StorageUsageInfo>;

  SiteDataSizeCollector(
      const base::FilePath& default_storage_partition_path,
      scoped_refptr<browsing_data::CookieHelper> cookie_helper,
      scoped_refptr<browsing_data::DatabaseHelper> database_helper,
      scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper,
      scoped_refptr<browsing_data::IndexedDBHelper> indexed_db_helper,
      scoped_refptr<browsing_data::FileSystemHelper> file_system_helper,
      scoped_refptr<browsing_data::ServiceWorkerHelper> service_worker_helper,
      scoped_refptr<browsing_data::CacheStorageHelper> cache_storage_helper);

  SiteDataSizeCollector(const SiteDataSizeCollector&) = delete;
  SiteDataSizeCollector& operator=(const SiteDataSizeCollector&) = delete;

  virtual ~SiteDataSizeCollector();

  using FetchCallback = base::OnceCallback<void(int64_t)>;

  // Requests to fetch the total storage space used by site data.
  void Fetch(FetchCallback callback);

 private:
  // Callback methods to be invoked when fetching the data is complete.
  void OnCookiesModelInfoLoaded(const net::CookieList& cookie_list);
  void OnDatabaseModelInfoLoaded(const DatabaseInfoList& database_info_list);
  void OnLocalStorageModelInfoLoaded(
      const LocalStorageInfoList& local_storage_info_list);
  void OnIndexedDBModelInfoLoaded(
      const IndexedDBInfoList& indexed_db_info_list);
  void OnFileSystemModelInfoLoaded(
      const FileSystemInfoList& file_system_info_list);
  void OnServiceWorkerModelInfoLoaded(
      const ServiceWorkerUsageInfoList& service_worker_info_list);
  void OnCacheStorageModelInfoLoaded(
      const CacheStorageUsageInfoList& cache_storage_info_list);

  // Callback for when the size is fetched from each storage backend.
  void OnStorageSizeFetched(int64_t size);

  // Path of the default storage partition of this profile.
  base::FilePath default_storage_partition_path_;

  // Pointers to the helper objects, needed to retreive all the types of locally
  // stored data.
  scoped_refptr<browsing_data::CookieHelper> cookie_helper_;
  scoped_refptr<browsing_data::DatabaseHelper> database_helper_;
  scoped_refptr<browsing_data::LocalStorageHelper> local_storage_helper_;
  scoped_refptr<browsing_data::IndexedDBHelper> indexed_db_helper_;
  scoped_refptr<browsing_data::FileSystemHelper> file_system_helper_;
  scoped_refptr<browsing_data::ServiceWorkerHelper> service_worker_helper_;
  scoped_refptr<browsing_data::CacheStorageHelper> cache_storage_helper_;

  // Callback called when sizes of all site data are fetched and accumulated.
  FetchCallback fetch_callback_;

  // Keeps track of how many fetch operations are ongoing.
  int in_flight_operations_;

  // Keeps track of the sum of all fetched size.
  int64_t total_bytes_;

  base::WeakPtrFactory<SiteDataSizeCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_BROWSING_DATA_SITE_DATA_SIZE_COLLECTOR_H_
